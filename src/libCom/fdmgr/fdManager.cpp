//
//      $Id$
//
//      File descriptor management C++ class library
//      (for multiplexing IO in a single threaded environment)
//
//      Author  Jeffrey O. Hill
//              johill@lanl.gov
//              505 665 1831
//
//      Experimental Physics and Industrial Control System (EPICS)
//
//      Copyright 1991, the Regents of the University of California,
//      and the University of Chicago Board of Governors.
//
//      This software was produced under  U.S. Government contracts:
//      (W-7405-ENG-36) at the Los Alamos National Laboratory,
//      and (W-31-109-ENG-38) at Argonne National Laboratory.
//
//      Initial development by:
//              The Controls and Automation Group (AT-8)
//              Ground Test Accelerator
//              Accelerator Technology Division
//              Los Alamos National Laboratory
//
//      Co-developed with
//              The Controls and Computing Group
//              Accelerator Systems Division
//              Advanced Photon Source
//              Argonne National Laboratory
//
//
//
//
//
// NOTES: 
// 1) This library is not thread safe
//
//

//
// ANSI C
//
#include <errno.h>
#include <string.h>

#define instantiateRecourceLib
#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "epicsThread.h"
#include "tsMinMax.h"
#include "fdManager.h"
#include "locationException.h"

#if defined ( _MSC_VER )
#   pragma warning ( push )
#   pragma warning ( disable: 4660 )
#endif

template class tsSLNode < fdReg >;
template class intId < unsigned int, 8, 32 >;
template class resTable < fdReg, fdRegId >;

#if defined ( _MSC_VER )
#   pragma warning ( pop )
#endif

epicsShareDef fdManager fileDescriptorManager;

//
// this must allow at least enough bits for all states 
// in type "fdRegType". The actual size of the table
// is used because it improves performance.
//
inline const unsigned fdRegId::minIndexBitWidth () 
{
    return fdManagerHashTableMinIndexBits;
}
inline const unsigned fdRegId::maxIndexBitWidth () 
{
    return fdManagerHashTableMaxIndexBits;
}

//
// fdManager::fdManager()
//
epicsShareFunc fdManager::fdManager () : pTimerQueue ( 0 )
{
    size_t i;
    int status;

    status = osiSockAttach ();
    assert (status);

    for ( i=0u; i < sizeof (this->fdSets) / sizeof ( this->fdSets[0u] ); i++ ) {
        FD_ZERO ( &this->fdSets[i] ); // X aCC 392
    }
    this->maxFD = 0;
    this->processInProg = 0u;
    this->pCBReg = 0;
}

//
// fdManager::~fdManager()
//
epicsShareFunc fdManager::~fdManager()
{
    fdReg   *pReg;

    while ( (pReg = this->regList.get()) ) {
        pReg->state = fdReg::limbo;
        pReg->destroy();
    }
    while ( (pReg = this->activeList.get()) ) {
        pReg->state = fdReg::limbo;
        pReg->destroy();
    }
    delete this->pTimerQueue;
    osiSockRelease();
}

//
// fdManager::process()
//
epicsShareFunc void fdManager::process (double delay)
{
    double minDelay;
    fdReg *pReg;
    struct timeval tv;
    int status;
    int ioPending = 0;

    this->lazyInitTimerQueue ();

    //
    // no recursion 
    //
    if (this->processInProg) {
        return;
    }
    this->processInProg = 1;

    //
    // One shot at expired timers prior to going into
    // select. This allows zero delay timers to arm
    // fd writes. We will never process the timer queue
    // more than once here so that fd activity get serviced
    // in a reasonable length of time.
    //
    minDelay = this->pTimerQueue->process(epicsTime::getCurrent());

    if ( minDelay >= delay ) {
        minDelay = delay;
    }

    tsDLIterBD < fdReg > iter = this->regList.firstIter ();
    while ( iter.valid () ) {
        FD_SET(iter->getFD(), &this->fdSets[iter->getType()]); 
        ioPending = 1;
        ++iter;
    }

    tv.tv_sec = static_cast<long> (minDelay);
    tv.tv_usec = static_cast<long> ((minDelay-tv.tv_sec)*epicsTime::uSecPerSec);

    /*
     * win32 requires this (others will
     * run faster with this installed)
     */
    if (!ioPending) {
        /*
         * recover from subtle differences between
         * windows sockets and UNIX sockets implementation
         * of select()
         */
        if (minDelay>0.0) epicsThreadSleep(minDelay);
        status = 0;
    }
    else {
        status = select (this->maxFD, &this->fdSets[fdrRead], 
            &this->fdSets[fdrWrite], &this->fdSets[fdrException], &tv);
    }

    this->pTimerQueue->process(epicsTime::getCurrent());
    if (status==0) {
        this->processInProg = 0;
        return;
    }
    else if (status<0) {
        int errnoCpy = SOCKERRNO;

        //
        // print a message if its an unexpected error
        //
        if (errnoCpy != SOCK_EINTR) {
            fprintf(stderr, 
            "fdManager: select failed because \"%s\"\n",
                SOCKERRSTR(errnoCpy));
        }

        this->processInProg = 0;

        return; 
    }

    //
    // Look for activity
    //
    iter=this->regList.firstIter ();
    while ( iter.valid () ) {
        tsDLIterBD<fdReg> tmp = iter;
        tmp++;
        if (FD_ISSET(iter->getFD(), &this->fdSets[iter->getType()])) {
            FD_CLR(iter->getFD(), &this->fdSets[iter->getType()]);
            this->regList.remove(*iter);
            this->activeList.add(*iter);
            iter->state = fdReg::active;
        }
        iter=tmp;
    }

    //
    // I am careful to prevent problems if they access the
    // above list while in a "callBack()" routine
    //
    while ( (pReg = this->activeList.get()) ) {
        pReg->state = fdReg::limbo;

        //
        // Tag current fdReg so that we
        // can detect if it was deleted 
        // during the call back
        //
        this->pCBReg = pReg;
        pReg->callBack();
        if (this->pCBReg != NULL) {
            //
            // check only after we see that it is non-null so
            // that we dont trigger bounds-checker dangling pointer 
            // error
            //
            assert (this->pCBReg==pReg);
            this->pCBReg = 0;
            if (pReg->onceOnly) {
                pReg->destroy();
            }
            else {
                this->regList.add(*pReg);
                pReg->state = fdReg::pending;
            }
        }
    }
    this->processInProg = 0;
}

//
// fdReg::destroy()
// (default destroy method)
//
epicsShareFunc void fdReg::destroy()
{
    delete this;
}

//
// fdReg::~fdReg()
//
epicsShareFunc fdReg::~fdReg()
{
    this->manager.removeReg(*this);
}

//
// fdReg::show()
//
epicsShareFunc void fdReg::show(unsigned level) const
{
    printf ("fdReg at %p\n", (void *) this);
    if (level>1u) {
        printf ("\tstate = %d, onceOnly = %d\n",
            this->state, this->onceOnly);
    }
    this->fdRegId::show(level);
}

//
// fdRegId::show()
//
void fdRegId::show ( unsigned level ) const
{
    printf ( "fdRegId at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 1u ) {
        printf ( "\tfd = %d, type = %d\n",
            this->fd, this->type );
    }
}

//
// fdManager::installReg ()
//
epicsShareFunc void fdManager::installReg (fdReg &reg)
{
    this->maxFD = tsMax(this->maxFD, reg.getFD()+1);
    this->regList.add (reg);
    reg.state = fdReg::pending;

    int status = this->fdTbl.add (reg);
    if (status!=0) {
        throwWithLocation ( fdInterestSubscriptionAlreadyExits () );
    }
}

//
// fdManager::removeReg ()
//
void fdManager::removeReg (fdReg &regIn)
{
    fdReg *pItemFound;

    pItemFound = this->fdTbl.remove (regIn);
    if (pItemFound!=&regIn) {
        fprintf(stderr, 
            "fdManager::removeReg() bad fd registration object\n");
        return;
    }

    //
    // signal fdManager that the fdReg was deleted
    // during the call back
    //
    if (this->pCBReg == &regIn) {
        this->pCBReg = 0;
    }
    
    switch (regIn.state) {
    case fdReg::active:
        this->activeList.remove (regIn);
        break;
    case fdReg::pending:
        this->regList.remove (regIn);
        break;
    case fdReg::limbo:
        break;
    default:
        //
        // here if memory corrupted
        //
        assert(0);
    }
    regIn.state = fdReg::limbo;

    FD_CLR(regIn.getFD(), &this->fdSets[regIn.getType()]);
}

//
// fdManager::reschedule ()
// NOOP - this only runs single threaded, and therefore they can only
// add a new timer from places that will always end up in a reschedule
//
void fdManager::reschedule ()
{
}

//
// lookUpFD()
//
epicsShareFunc fdReg *fdManager::lookUpFD (const SOCKET fd, const fdRegType type)
{
    if (fd<0) {
        return NULL;
    }
    fdRegId id (fd,type);
    return this->fdTbl.lookup(id); 
}

//
// fdReg::fdReg()
//
fdReg::fdReg (const SOCKET fdIn, const fdRegType typIn, 
        const bool onceOnlyIn, fdManager &managerIn) : 
    fdRegId (fdIn,typIn), state (limbo), 
    onceOnly (onceOnlyIn), manager (managerIn)
{ 
    if (!FD_IN_FDSET(fdIn)) {
        fprintf (stderr, "%s: fd > FD_SETSIZE ignored\n", 
            __FILE__);
        return;
    }
    this->manager.installReg (*this);
}

