/* Copyright (c) 2012, Jason Lloyd-Price
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */


#include "worker.h"
#include <iostream>

#include <SDL_thread.h>
#include <SDL_mutex.h>

#ifdef _WINDOWS
#include <windows.h>
#endif

Worker::Worker()
{
	workToDo = SDL_CreateSemaphore( 0 );
	queueLock = SDL_CreateMutex();

	endWorker = false;
	thread = SDL_CreateThread( workerFunc, "Eihort Worker", this );
}

Worker::~Worker() {
	SDL_DestroySemaphore( workToDo );
	SDL_DestroyMutex( queueLock );
}

void Worker::doTask( Executor exec, void *cookie ) {
	// Add the task to the queue
	SDL_LockMutex( queueLock );
	Task t = { exec, cookie };
	queue.push_back( t );
	SDL_UnlockMutex( queueLock );

	// Unlock the worker
	SDL_SemPost( workToDo );
}

void Worker::killWorker() {
	doTask( &killMe, this );
}

int Worker::workerFunc( void *worker_ ) {
#ifdef _WINDOWS
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
#endif

#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
    // POSIX w/ REALTIME & THREADS extensions
  int sched_policy, sched_prio_min;
  struct sched_param sched_param;
  if (pthread_getschedparam(pthread_self(), &sched_policy, &sched_param) == 0 &&
    (sched_prio_min = sched_get_priority_min(sched_policy)) != -1)
  {
    // Lower thread priority
    sched_param.sched_priority = (sched_param.sched_priority + sched_prio_min) / 2;
    pthread_setschedparam(pthread_self(), sched_policy, &sched_param);
  }
#endif


	Worker *worker = (Worker*)worker_;

	while( !worker->endWorker ) {
		SDL_SemWait( worker->workToDo );
		SDL_LockMutex( worker->queueLock );
		Task t = worker->queue.front();
		worker->queue.pop_front();
		SDL_UnlockMutex( worker->queueLock );

		t.exec( t.cookie );
	}

	delete worker;
	return 0;
}

void Worker::killMe( void *worker_ ) {
	Worker *worker = (Worker*)worker_;
	worker->endWorker = true;
}
