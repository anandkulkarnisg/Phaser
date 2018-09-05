#include<iostream>
#include<string>

#include "Phaser.h"

using namespace std;

// First we implement the Phaser constructor which takes no parameters. It simply creates a Phaser with all values defaulted.
Phaser::Phaser() : m_count(0), m_parties(0), m_phase(0), m_status(false)
{

}

// We now implement the Parametered Constructor. This takes a certain number of parties as input and registers them.
Phaser::Phaser(const int& parties) : m_phase(0), m_count(0), m_status(false)
{
	// First we check if the parties count is sane. If not then we throw exception for IllegalArgument.
	if(parties<=0)
		throw std::string("This is a IllegalArgument. Please verify the parties number passed.");

	// If the number is fine then we simply increment the m_parties.
	m_parties = parties;
}

// We now implement the breakPhaser method. This is used internally to break the Phaser once abnormal condition is met. It simply sets the status m_status to true and notifies waiting threads.
void Phaser::breakPhaser()
{
	// When phaser is broken we set the Phase to -ve value. This is required by the standard.
	m_phase = -INT_MAX;
	m_status = true;	
	m_cond.notify_all();	
}

// We now implement internal method of nextPhaseGeneration. This is invoked when last thread to arrive has come for current phase and m_count has hit zero. We need to continue to next phase.
void Phaser::nextPhaseGeneration()
{
	// When next generation is to be created all we need to do is. Increment the m_phase. copy the value of m_parties to m_count. notify waiting threads.
	if(m_phase == INT_MAX)
		m_phase = 0;
	else
		++m_phase;
	m_count = m_parties;
	m_cond.notify_all();
}

// We now implement the internal method of deregisterInternal. This reduces the count of the m_parties.
void Phaser::deRegisterInternal()
{
	--m_parties;
	if(m_parties == 0)	
		breakPhaser();
}

// We now implement the internal method of awaitAdvanceInternal. This simply awaits for a given phase number to be over till Phaser switches to next phase.
// Awaits the phase of this phaser to advance from the given phase value, returning immediately if the current phase is not equal to the given phase value or this phaser is terminated.
// Parameters: phase - an arrival phase number, or negative value if terminated; this argument is normally the value returned by a previous call to arrive or arriveAndDeregister.
// Returns: the next arrival phase number, or the argument if it is negative, or the (negative) current phase if terminated  

int Phaser::awaitAdvanceInternal(const int& phase)
{
	if(phase<0)
		return(phase);
	if(m_phase!=phase || m_status)
		return(m_phase);
	int currentPhase = phase;
	unique_lock<mutex> exclusiveLock(m_mutex);
	m_cond.wait(exclusiveLock, [&](){ return((currentPhase!=m_phase) || m_status); });	
	// If we came out two possibilities.if Phaser is broken , or phase has moved on. either case m_phase return is sufficient.
	return(m_phase);
}

// The arrive method implementation is required by many public methods. Ex:- arrive , arriveAndDeregister , arriveAndAwaitAdvance. Hence It looks logical to implement the arrive method internally and
// call the internal method via wrappers of public methods. 
int Phaser::arriveInternal()
{
	// First we need to verify if we are in sane condition. i.e check that the Phaser is in stable condition else throw IllegalStateException.
	if(m_count<0 || m_parties == 0 || m_phase<0 || m_status)
	{
		breakPhaser();
		throw std::string("Exception : The phaser is in broken state. Please verify.");
	}

	// We have to take a Lock here because even though we have atomic variables we need synchronization.
	unique_lock<mutex> exclusiveLock(m_mutex);

	// Now we aim to count down the m_count to indicate the arrival.
	if(m_count>0)
		--m_count;
	else
	{
		// If m_count is zero then this is a rogue arrival without any registration. We break the Phaser and set the termination status. notify and quit.
		breakPhaser();
		return(m_phase);
	}
	// We now check if m_count is zero. That means we finished the current phase and we need to 1) notify 2) increment the current phase number. 
	int localPhase = m_phase;
	if(m_count == 0)
		nextPhaseGeneration();

	// return the value.
	return(localPhase);
}

// Implement the arrive method now as a specific wrapper of the arriveInternal Method.
int Phaser::arrive()
{
	int returnStatus = arriveInternal();
	if(m_status)
		throw std::string("IllegalStateException");
	return(returnStatus);
}

// Implement the arrive and deregister method. This implements arrive logic followed by reduction of the m_parties.
int Phaser::arriveAndDeregister()
{
	int returnStatus = arriveInternal();
	if(m_status)
		throw std::string("IllegalStateException");		
	deRegisterInternal();
	if(m_status)
		throw std::string("IllegalStateException");	
	return(returnStatus);
}

// Implement the public method of awaitAdvance. This method simply wraps around the awaitAdvanceInternal method.
int Phaser::awaitAdvance(const int& phase)
{
	int returnPhase=awaitAdvanceInternal(phase);
	return(returnPhase);
}

// We now implement the public method of arriveAndAwaitAdvance. This method has below requirement.
// Arrives at this phaser and awaits others. Equivalent in effect to awaitAdvance(arrive()). If you need to await with interruption or timeout, 
// you can arrange this with an analogous construction using one of the other forms of the awaitAdvance method. 
// If instead you need to deregister upon arrival, use awaitAdvance(arriveAndDeregister()).
// It is a usage error for an unregistered party to invoke this method. However, this error may result in an IllegalStateException only upon some subsequent operation on this phaser, if ever.
// Returns: The arrival phase number, or the (negative) current phase if terminated.

int Phaser::arriveAndAwaitAdvance()
{
	// First we need to do the arrival bit.
	int returnPhase = arriveInternal();
	if(m_status)
		throw std::string("IllegalStateException");
	// We are here means we simply wait for phase advancement.
	awaitAdvance(returnPhase);
	if(!m_status)
		return(returnPhase);
	else
		return(m_phase);
}

// We implement the bulkRegister method here. The bulk register method essentially allows to register more than 1 number of parties in one go.
// Adds the given number of new unarrived parties to this phaser. If an ongoing invocation of onAdvance(int, int) is in progress, this method may await its completion before returning. 
// If this phaser has a parent, and the given number of parties is greater than zero, and this phaser previously had no registered parties, this child phaser is also registered with its parent. 
// If this phaser is terminated, the attempt to register has no effect, and a negative value is returned.
// Parameters: parties - the number of additional parties required to advance to the next phase
// Returns: the arrival phase number to which this registration applied. If this value is negative, then this phaser has terminated, in which case registration has no effect.
// Throws: IllegalStateException - if attempting to register more than the maximum supported number of parties
// IllegalArgumentException - if parties < 0 

int Phaser::bulkRegister(const int& parties)
{
	if(parties==INT_MAX||parties<0)
		throw std::string("IllegalArgumentException");

	// We first attempt to take a Lock. Then once acquired check if the Phaser is in good condition. if so proceed else kick out. 
	unique_lock<mutex> exclusiveLock(m_mutex);
	if(!m_status)
	{
		m_parties+=parties;
		return(m_phase);
	}
	else
		return(m_phase);
}

// We now implement the register method.Adds a new unarrived party to this phaser. If an ongoing invocation of onAdvance(int, int) is in progress, 
// This method may await its completion before returning. If this phaser has a parent, and this phaser previously had no registered parties, 
// This child phaser is also registered with its parent. If this phaser is terminated, the attempt to register has no effect, and a negative value is returned.
// Returns: The arrival phase number to which this registration applied. If this value is negative, then this phaser has terminated, in which case registration has no effect.
// Throws: IllegalStateException - if attempting to register more than the maximum supported number of parties.
// register is a keyword hence we use doRegister method name.

int Phaser::doRegister()
{
	int returnStatus=bulkRegister(1);
	return(returnStatus);
}

