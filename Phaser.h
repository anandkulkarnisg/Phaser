#include<iostream>
#include<string>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<atomic>
#include<climits>

#ifndef Phaser_H
#define Phaser_H

class Phaser
{
	private:
		int m_count;							// This is the core internal count doing counting for the Phaser.
		int m_parties;							// Track the number of parties required for phaser dynamically.
		int m_phase;							// Keep track of which phase are we in.
		bool m_status;							// status of the Phaser. is set to true when Phaser is terminated.
		std::mutex m_mutex;						// Used to synchronize threads.
		std::condition_variable m_cond;			// Signalling across threads on events.
		int arriveInternal();					// boolean is needed because we want to know if we want to wait or simply keep moving.
		void breakPhaser();						// break the phaser. i.e set the broken status to true and notify all waiting threads.
		void nextPhaseGeneration();				// Advance the Phaser to next generation of phase once the current phase is over.
		void deRegisterInternal();				// Internal private method to reduce parties count.
		int awaitAdvanceInternal(const int&);	// Internal private method to await for advancement of from current phase to next or other event.

	protected:
		// This has to be overriden and implelemented by the application method i.ewhat to run after a phase completion.
		bool onAdvance(const int& phase, const int& registeredParties);	

	public:
		Phaser();
		Phaser(const int&);

		int arrive();
		int arriveAndAwaitAdvance();
		int arriveAndDeregister();
		int awaitAdvance(const int&);
		int bulkRegister(const int&);
		void forceTermination();
		int getArrivedParties();
		int getPhase();
		int getRegisteredParties();	
		int getUnarrivedParties();
		bool isTerminated();
		int doRegister();
		std::string toString();
};		

#endif
