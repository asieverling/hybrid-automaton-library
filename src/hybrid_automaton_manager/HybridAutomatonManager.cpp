/* RoboticsLab, Copyright 2008-2010 SimLab Co., Ltd. All rights reserved.
*
* This library is commercial and cannot be redistributed, and/or modified
* WITHOUT ANY ALLOWANCE OR PERMISSION OF SimLab Co., LTD.
*/
#include "HybridAutomatonManager.h"
#include "HybridAutomatonManagerCmd.h"

#include "XMLDeserializer.h"
#include "msgs\String.h"
#include "msgs\Transform.h"

#include "controllers\include\TPImpedanceControlSet.h"

#include <process.h>

unsigned __stdcall deserializeHybridAutomaton(void *udata)
{
	DeserializingThreadArguments* thread_args = static_cast<DeserializingThreadArguments*>(udata);
	HybridAutomaton* ha = NULL;
	try {
		ha = XMLDeserializer::createHybridAutomaton(thread_args->_string, thread_args->_sys, thread_args->_dT);
		//std::cout << "--------------- recieving hybrid automaton ------------------" << std::endl;
		//std::cout << ha->toStringXML() << std::endl;
	}
	catch(std::string e)
	{
		std::cerr << "[deserializeHybridAutomaton] ERROR while deserializing hybrid automaton!" << std::endl;
		std::cerr << e << std::endl;

		delete ha;
		delete thread_args;
		return 0;
	}

	WaitForSingleObject(*(thread_args->_deserialize_mutex), INFINITE);
	if(thread_args->_noQueue)
	{
		for(size_t i=0;i<thread_args->_deserialized_hybrid_automatons->size();i++)
		{
			delete thread_args->_deserialized_hybrid_automatons->at(i);
		}
		thread_args->_deserialized_hybrid_automatons->clear();	
	}

	thread_args->_deserialized_hybrid_automatons->push_back(ha);
	ReleaseMutex(*(thread_args->_deserialize_mutex));
	
	delete thread_args;

	return 0;
}

std::vector<double> convert(const dVector& in)
{
	std::vector<double> out(in.size());
	for(int i = 0; i < in.size(); ++i)
		out[i] = in[i];
	return out;
}

HybridAutomatonManager::HybridAutomatonManager(rDC rdc) 
:rControlAlgorithm(rdc)
, _sys(NULL)
, _blackboard(NULL)
, _activeMotionBehaviour(NULL)
, _dof(0)
, _servo_on(false)
, _hybrid_automaton(NULL)
, _criterion(new LocalDecisionCriterion())
, _active(false)
, _noQueue(false)
{
	_deserialize_mutex = CreateMutex(0, FALSE, 0);
	if( !_deserialize_mutex ) {
		throw(std::string("[HybridAutomatonManager::HybridAutomatonManager] ERROR: Mutex was not created (_deserialize_mutex)!"));
	}

#ifdef USE_LOCALIZATION
	_qLoc.resize(10);
#endif
}

HybridAutomatonManager::~HybridAutomatonManager()
{
	if(_activeMotionBehaviour)
		delete _activeMotionBehaviour;

	if(_blackboard)
		delete _blackboard;

    if(_criterion)
		delete _criterion;

	//delete _defaultMotionBehavior;

	FREE_SYSTEMS();
}

void HybridAutomatonManager::init(int mode)
{
	_sys = LOAD_SYSTEM(_path, _aml, _T0, _q0);
	assert(_sys);

	//std::cerr << " ROBOT " << _sys << std::endl;

	_dof = _sys->jointDOF() + _sys->earthDOF() + _sys->constraintDOF();

	//Initialize variables
	_q.resize(_dof);
	_qOld.resize(_dof);
	_qdot.resize(_dof);
	_torque.resize(_dof);

	_q.zero();
	_qOld.zero();
	_torque.zero();
	_qdot.zero();

	//Find the robot device. This devicce is used to read position and write torques
	//to the robot
	_robot = findDevice(_T("ROBOT"));
	RASSERT(_robot != INVALID_RHANDLE);

	gravityCompensation();
}

void HybridAutomatonManager::gravityCompensation()
{
	TPImpedanceControlSet* nullControl = new TPImpedanceControlSet(this->_sys, this->_dT);
	nullControl->setGravity(0, 0, -GRAV_ACC);
	
	_activeMotionBehaviour = new MotionBehaviour(new Milestone(), new Milestone(),nullControl, 0.0);
	_activeMotionBehaviour->activate();
}

void HybridAutomatonManager::activateBlackboard(std::string &rlab_host, int rlab_port, std::string &ros_host, int ros_port)
{
	_blackboard = RTBlackBoard::getInstance(rlab_host, rlab_port, ros_host, ros_port);
	
#ifdef USE_LOCALIZATION

	//Wait until localization messages are updated
	while(!_blackboard->exists("/odom"))
	{
		std::cout<<"waiting for blackboard"<<std::endl;
		_blackboard->subscribeToTransform("/odom", "/map");
		updateBlackboard();
		Sleep(500);
	}
#endif

}

void HybridAutomatonManager::setCollisionInterface(CollisionInterface* collision_interface)
{
	CollisionInterface::instance = collision_interface;
}

void HybridAutomatonManager::setLocalDecisionCriterion(LocalDecisionCriterion* criterion)
{
	delete _criterion;
	_criterion=criterion;
}

void HybridAutomatonManager::setHybridAutomaton(std::string _new_hybrid_automaton_str)
{
	DeserializingThreadArguments* thread_args = new DeserializingThreadArguments();
	thread_args->_sys = this->_sys;
	thread_args->_string = _new_hybrid_automaton_str;
	thread_args->_dT = this->_dT;
	thread_args->_noQueue = this->_noQueue;
	thread_args->_deserialize_mutex = &(this->_deserialize_mutex);
	thread_args->_deserialized_hybrid_automatons = &(this->_deserialized_hybrid_automatons);

	if (_beginthreadex(NULL, 0, deserializeHybridAutomaton, (void*)thread_args, 0, NULL) == 0)
	{
		std::cerr << "[HybridAutomatonManager::updateHybridAutomaton] Error creating thread to deserialize xml string!" << std::endl;
	}
}

void HybridAutomatonManager::setHybridAutomaton(HybridAutomaton* _new_hybrid_automaton)
{
	WaitForSingleObject(_deserialize_mutex, INFINITE);
	this->_deserialized_hybrid_automatons.push_back(_new_hybrid_automaton);
	ReleaseMutex(_deserialize_mutex);
}

void HybridAutomatonManager::update(const rTime& t)
{
	this->updateHybridAutomaton();

	rControlAlgorithm::update(t);

	this->updateBlackboard();
}

void HybridAutomatonManager::updateBlackboard()
{
	if(!_blackboard)
		return;

	_blackboard->setJointState("/joint_state", _sys->q(), _sys->qdot(), _torque);

	HTransform relative_transform;
	rxBody* end_effector = _sys->getUCSBody(_T("EE"), relative_transform);
	HTransform absolute_transform = end_effector->T() * relative_transform;
	_blackboard->setTransform("ee", absolute_transform, "base_link");

#ifdef USE_LOCALIZATION
	std::vector<double> b_position;
	std::vector<double> b_velocity;
	std::vector<double> b_effort;
	for(int i=7; i<10; i++)
	{
		b_position.push_back(_q[i]);
		b_velocity.push_back(_qdot[i]);
		b_effort.push_back(0.0);
	}
	_blackboard->setJointState("/xr4000_jointstate", b_position, b_velocity, b_effort);

	if(_blackboard->isUpdated("/odom"))
	{
		// to odom

		rlab::Transform* localT = dynamic_cast<rlab::Transform*>(_blackboard->getData("/odom"));
		
		HTransform locFrame;
		locFrame.r = Displacement(localT->getPosition()[0],localT->getPosition()[1],localT->getPosition()[2]);
		locFrame.R = Rotation(localT->getOrientation()[0],localT->getOrientation()[1],localT->getOrientation()[2],localT->getOrientation()[3]);		
			
		double maxLocalizationUpdate = 0.01;
		double maxLocalizationUpdateTheta = 0.01;

		double updateX = localT->getPosition()[0]-_localizedFrame.r(0);
		double updateY = localT->getPosition()[1]-_localizedFrame.r(1);
		double updateTheta = acos(locFrame.R(0,0)) - acos(_localizedFrame.R(0,0));

		if( fabs(updateX) < 0.2 && fabs(updateY) < 0.2 && fabs(updateTheta) < 0.2 )
		{
			if(updateX > 0.0)
				updateX = std::max(updateX, maxLocalizationUpdate);
			else
				updateX = std::min(updateX, -maxLocalizationUpdate);

			if(updateY > 0.0)
				updateY = std::max(updateY, maxLocalizationUpdate);
			else
				updateY = std::min(updateY, -maxLocalizationUpdate);

			if(updateTheta > 0.0)
				updateTheta = std::max(updateTheta, maxLocalizationUpdateTheta);
			else
				updateTheta = std::min(updateTheta, -maxLocalizationUpdateTheta);

			_localizedFrame.Reset();
			_localizedFrame.r[0] = updateX;
			_localizedFrame.r[1] = updateY;
			_localizedFrame.R.Set(updateTheta, 0.0, 0.0, ZYX);
		}
		else
		{
			//We have a large localization update - something went wrong
			std::cout<<"Localization error exceeded 20cm! - update IGNORED!!!"<<std::endl;
		}
	}
#endif
	
	_blackboard->step();
	
}

void HybridAutomatonManager::updateHybridAutomaton()
{
	if (_blackboard && _blackboard->isUpdated("update_hybrid_automaton"))
	{
		rlab::String* ha_xml = dynamic_cast<rlab::String*>(_blackboard->getData("update_hybrid_automaton"));
		try
		{
			DeserializingThreadArguments* thread_args = new DeserializingThreadArguments();
			thread_args->_sys = this->_sys;
			thread_args->_string = ha_xml->get();
			thread_args->_dT = this->_dT;
			thread_args->_noQueue= this->_noQueue;
			thread_args->_deserialize_mutex = &(this->_deserialize_mutex);
			thread_args->_deserialized_hybrid_automatons = &(this->_deserialized_hybrid_automatons);

			if (_beginthreadex(NULL, 0, deserializeHybridAutomaton, (void*)thread_args, 0, NULL) == 0)
			{
				std::cerr << "[HybridAutomatonManager::updateHybridAutomaton] Error creating thread to deserialize xml string!" << std::endl;
			}
		}
		catch(::std::string e)
		{
			std::cout << "[HybridAutomatonManager::updateHybridAutomaton] Error caught: " << std::endl;
			::std::cout << e << ::std::endl;
		}
	}
}

void HybridAutomatonManager::setNominalSystem(const TCHAR* path, const TCHAR* aml, const HTransform& T0, const dVector& q0)
{
	this->_path = path;
	this->_aml = aml;
	this->_T0 = T0;
	this->_q0 = q0;
}

void HybridAutomatonManager::setPeriod(const rTime& dT)
{
	this->_dT = dT;
}


void HybridAutomatonManager::_readDevices()
{	
	_qOld = _q;

	int r = readDeviceValue(_robot, &_q[0], sizeof(double) * _q.size(), 0);
	assert (r == sizeof(double) * _q.size());

}


void HybridAutomatonManager::_writeDevices()
{
	int w = writeDeviceValue(_robot, &_torque[0], sizeof(double) * _torque.size());
	assert(w == sizeof(double)*_torque.size());
}


void HybridAutomatonManager::_reflect()
{
	
#ifdef USE_LOCALIZATION
	HTransform odoT;
	odoT.r = Displacement(_q[7], _q[8], 0.0);
	odoT.R = Rotation(_q[9],0.0,0.0);
	HTransform newT = _localizedFrame*odoT;
	
	_qLoc = _q;
	_qLoc[7]=newT.r(0);
	_qLoc[8]=newT.r(1);
	double angLoc;
	Vector3D dir;
	newT.R.GetEquivalentAngleAxis(dir,angLoc);
	
	if(dir[2]<0)
	{
		//can this happen?
		std::cout<<"axis switched"<<std::endl;
		angLoc *= -1.0;
	}

	_qLoc[9]=angLoc;

	_sys->q(_qLoc);

#else	
	_sys->q(_q);
#endif
	_sys->qdot(_qdot);
}

void HybridAutomatonManager::_compute(const double& t)
{
	if (_servo_on)
	{
		_torque = _activeMotionBehaviour->update(t);
	}
	else 
	{
		_torque.zero();
	}

	if(!_active)
		return;

	//Now switch the active motion behaviour if possible
    Milestone* childMs=(Milestone*)(_activeMotionBehaviour->getChild());
	Milestone* queryMs; //The milestone we choose our next motion from

	
	//Flag tells if the graph structure changed.	
	bool behaviourChange = true;

	if (!_deserialized_hybrid_automatons.empty() && WaitForSingleObject(_deserialize_mutex, 0) != WAIT_FAILED)
	{
		delete _hybrid_automaton;
		_hybrid_automaton = _deserialized_hybrid_automatons.front();

		_deserialized_hybrid_automatons.pop_front();
		
		//std::cout << "[HybridAutomatonManager::_compute] INFO: Switching Hybrid Automaton" << std::endl;

		ReleaseMutex(_deserialize_mutex);

		queryMs = _hybrid_automaton->getStartNode(); //Take the first behaviour from the new roadmap
		behaviourChange = true;
	}
	else if(childMs->hasConverged(_sys)) //The current behaviour converged
	{
		std::cout<<"[HybridAutomatonManager::_compute] INFO: Milestone "<<childMs->getName()<<" converged."<<std::endl;
		queryMs = childMs; //Converged, switch to next milestone
		behaviourChange = true;

		if(childMs->getName() == "goal")
		{
			_active = false;
			return;
		}
	}
	else	
	{
		queryMs = (Milestone*)_activeMotionBehaviour->getParent(); //Switch before reaching the milestone.
		behaviourChange = false;		
	}

	// make new local decision:
	if(_hybrid_automaton)
	{
		MotionBehaviour* nextMotion; 
		nextMotion = _criterion->getNextMotionBehaviour(queryMs,_hybrid_automaton, behaviourChange, t, _sys->q());

		//Switch motion behaviour
		if(nextMotion && nextMotion != _activeMotionBehaviour)
		{
			if(nextMotion->isUpdateAllowed() && _activeMotionBehaviour->isUpdateAllowed())
			{
				dVector delta = nextMotion->getGoal() - _activeMotionBehaviour->getGoal();

				//Try to update currently running behaviour instead of replacing it
				if(delta.norm() > 0.01)
				{
					_activeMotionBehaviour->updateControllers(nextMotion);
					Milestone* next=(Milestone*)(nextMotion->getChild());
					std::cout << "[HybridAutomatonManager::_compute] INFO: Updated goal towards "<< next->getName() << " ,x="<<next->getConfiguration()[7]<<", y="<<next->getConfiguration()[8]<<std::endl;
				}
			}
			else
			{
				//update not possible (controller types changed) - exchange motion behaviour
				_activeMotionBehaviour->deactivate();
				_activeMotionBehaviour = nextMotion;
				_activeMotionBehaviour->activate();
				std::cout << "[HybridAutomatonManager::_compute] INFO: Changed control set. Next goal: "<< ((Milestone*)(nextMotion->getChild()))->getName() << std::endl;			
			}
		}
	}
}



void HybridAutomatonManager::_estimate()
{
	//We calculate the velocity here. This might not be the best option.
	_qdot  = _q - _qOld;
	_qdot /= _dT;
}

int HybridAutomatonManager::command(const short& cmd, const int& arg)
{
	switch (cmd)
	{
	case SERVO_ON:
		{
			_servo_on = !_servo_on;
			std::cout << "[HybridAutomatonManager::command] Servo ON: " << _servo_on << std::endl;
		}
		break;

	case ACTIVATE:
		{
			_active = !_active;
		}
		break;
		
	case BLACKBOARD_ON:
		{
			std::map<int, std::string> domain_names;
			domain_names[URI_LOCAL] = "";
			domain_names[URI_BOTTOM_1] = "130.149.238.178";
			domain_names[URI_BOTTOM_2] = "130.149.238.179";
			domain_names[URI_BOTTOM_3] = "130.149.238.180";
			domain_names[URI_LOHENGRIN] = "130.149.238.186";
			domain_names[URI_HASMA] = "130.149.238.184";
			domain_names[URI_LEIBNIZ] = "130.149.238.185";
            domain_names[URI_POSEIDON] = "130.149.238.193";
			domain_names[URI_FIRSTMM] = "130.149.238.220";
			domain_names[URI_SHOEFER] = "130.149.238.182";
			domain_names[URI_RBO_EXTRA] = "130.149.238.188";

			int bit_code = arg;
			
			int port = bit_code >> 16;
			bit_code &= ~(port << 16);
			
			int local_host = bit_code >> 8;
			bit_code &= ~(local_host << 8);

			int remote_host = bit_code;
			
			std::cout << "Enabled BlackBoard. Establishing connection between " << domain_names[local_host] << ":" << port << " and " << domain_names[remote_host] << ":" << port << "." << std::endl;	
			
			if (local_host != URI_LOCAL && domain_names[local_host].empty() ||
				remote_host != URI_LOCAL && domain_names[remote_host].empty())
			{
				std::cout << "WARNING! Unknown host: " << local_host << " or " << remote_host << "! (Will only establish unconnected BlackBoard)" << std::endl;
			}

			activateBlackboard(domain_names[local_host], port, domain_names[remote_host], port);
		}
		break;
	default:
		break;
	}

	return 0;
}


void HybridAutomatonManager::datanames(vector<string_type>& names, int channel)
{
	switch (channel) {
		case 1: names.push_back(_T("Torque"));
		case 2: names.push_back(_T("Q"));
		case 3: names.push_back(_T("Velocity"));
	}
}

void HybridAutomatonManager::collect(vector<double>& data, int channel)
{
	if (channel == PLOT_TORQUE)
	{
		for(int i = 0; i < _dof; ++i)
			data.push_back(_torque[i]);
	}
	else if (channel == PLOT_Q)
	{
		for(int i = 0; i < _dof; ++i)
			data.push_back(_sys->q()[i]);
	}
	else if (channel == PLOT_VELOCITY)
	{
		for(int i = 0; i < _dof; ++i)
			data.push_back(_sys->qdot()[i]);
			//data.push_back(_activeMotionBehaviour->getDesired()[i]);
	}	
	else if (channel == PLOT_EEFRAME)
	{
		HTransform relative_transform;
		rxBody* end_effector = _sys->getUCSBody(_T("EE"), relative_transform);
		HTransform absolute_transform = end_effector->T() * relative_transform;
		for(int i = 0; i < 3; ++i)
			data.push_back(absolute_transform.r(i));
	}	
}

void HybridAutomatonManager::onSetInterestFrame(const TCHAR* name, const HTransform& T)
{
}

rControlAlgorithm* CreateControlAlgorithm(rDC& rdc)
{
	return new HybridAutomatonManager(rdc);
}
