#include "MotionBehaviour.h"
#include <iostream>
#include <string>

#include "XMLDeserializer.h"


//#define NOT_IN_RT

using namespace std;

MotionBehaviour::MotionBehaviour() :
Edge<Milestone>(NULL, NULL, -1)
, control_set_(NULL)
, robot_(NULL)
, time_(-1)
, dT_(-1)
, max_velocity_(-1)
, min_time_(-1)
, time_to_converge_(0.0)
{
}

MotionBehaviour::MotionBehaviour(const Milestone * dad, const Milestone * son, rxSystem* robot, double weight, double dt ):
Edge<Milestone>(dad, son, weight)
, robot_(robot)
, time_(0)
, dT_(dt)
, max_velocity_(-1)
, min_time_(-1)
, time_to_converge_(0.0)
{
	if(robot_)
	{

		control_set_ = new rxControlSet(robot_, dT_);
		control_set_->setGravity(0,0,-GRAV_ACC);
		control_set_->setInverseDynamicsAlgorithm(new rxAMBSGravCompensation(this->robot_));
		control_set_->nullMotionController()->setGain(0.02,0.0,0.01);
	}
	else
	{
		control_set_= NULL;
	}
}

MotionBehaviour::MotionBehaviour(const Milestone * dad, const Milestone * son, rxControlSetBase* control_set, double weight ):
Edge<Milestone>(dad, son, weight)
, time_(0)
, max_velocity_(-1)
, min_time_(-1)
, time_to_converge_(0.0)
, control_set_(control_set)
{
	if(control_set_)
	{
		dT_ = control_set->dt();
		robot_ = const_cast<rxSystem*>(control_set->sys());

		// TODO: should this be done by the caller?
		control_set_->setGravity(0,0,-GRAV_ACC);
		control_set_->setInverseDynamicsAlgorithm(new rxAMBSGravCompensation(this->robot_));
		control_set_->nullMotionController()->setGain(0.02,0.0,0.01);
	}
	else
	{
		dT_ = 0.002;
		robot_ = NULL;
	}
}

MotionBehaviour::MotionBehaviour(const MotionBehaviour & motion_behaviour_copy) :
Edge(motion_behaviour_copy),
robot_(motion_behaviour_copy.robot_),
time_(motion_behaviour_copy.time_),
dT_(motion_behaviour_copy.dT_),
min_time_(motion_behaviour_copy.min_time_),
max_velocity_(motion_behaviour_copy.max_velocity_),
goal_controllers_(motion_behaviour_copy.goal_controllers_),
time_to_converge_(motion_behaviour_copy.time_to_converge_)
{
	// NOTE (Roberto): Should we avoid copying the value of time_? We are creating a new MB, maybe time_ should be 0
	if(motion_behaviour_copy.control_set_)
	{
		// NOTE (Roberto): Two ugly options:
		// Use typeid(control_set_).name() as we do when serializing
		// Drawback: typeid has no standard behaviour
		// Use dynamic_cast to different types and check if the resulting pointer is NULL (not possible to cast->
		// it is not of this type) or not NULL (possible to cast -> of this type)
		// Drawback: we can always cast to base classes, we should check only if we can to derived ones
		// Implemented: mixture of both :(
		// CHANGE: Use dynamic_cast tree, because now control_set_ is a rxControlSetBase*, and that is what we 
		// get if we use typeid
		//
		//std::string control_set_name = std::string(typeid(motion_behaviour_copy.control_set_).name());
		//std::string control_set_name2 = control_set_name.substr(6);		//Ignore the 'class ' part
		//control_set_name2.resize(control_set_name2.size()-2);	//Ignore the ' *' part
		//if(control_set_name2 == std::string("rxControlSet"))
		//{
		//	this->control_set_ = new rxControlSet((*dynamic_cast<rxControlSet*>(motion_behaviour_copy.control_set_)));
		//}
		//else if(control_set_name2==std::string("rxTPOperationalSpaceControlSet"))
		//{
		//	this->control_set_ = new rxTPOperationalSpaceControlSet((*dynamic_cast<rxTPOperationalSpaceControlSet*>(motion_behaviour_copy.control_set_)));
		//}
		//else // Use the base class if there is something wrong
		//{
		//	this->control_set_ = new rxControlSet((*dynamic_cast<rxControlSet*>(motion_behaviour_copy.control_set_)));
		//}
		if(dynamic_cast<rxTPOperationalSpaceControlSet*>(motion_behaviour_copy.control_set_))
		{
			this->control_set_ = new rxTPOperationalSpaceControlSet((*dynamic_cast<rxTPOperationalSpaceControlSet*>(motion_behaviour_copy.control_set_)));
		}
		else if(dynamic_cast<rxControlSet*>(motion_behaviour_copy.control_set_))
		{
			this->control_set_ = new rxControlSet((*dynamic_cast<rxControlSet*>(motion_behaviour_copy.control_set_)));
		}
	}
	else
	{
		this->control_set_ = NULL;
	}
}

MotionBehaviour::~MotionBehaviour() 
{
	// Do not delete any object that was not directly created in the constructor!!
	if(control_set_)
	{
		//TODO: Do not delete it! It was already deleted somehow -> Where?
		//delete control_set_;
		//control_set_ = NULL;
	}
}

void MotionBehaviour::addController(rxController* ctrl, bool is_goal_controller) 
{
	if(ctrl){
		if(ctrl->dt() != this->dT_)
		{
			throw std::string("[MotionBehaviour::addController] ERROR: Time intervals of MotionBehaviour and all its controllers must be the same.");
		}
		ctrl->deactivate();
		goal_controllers_[ctrl->name()] = is_goal_controller;
		control_set_->addController(ctrl, ctrl->name(), ctrl->priority());
	}
	else
	{
		throw std::string("[MotionBehaviour::addController] ERROR: The pointer to rxController to be added is NULL.");
	}				
}

void MotionBehaviour::activate() 
{
	if (control_set_)
	{
		std::list<rxController*> controllers = control_set_->getControllers();
		for(std::list<rxController*>::const_iterator it = controllers.begin(); it != controllers.end(); ++it)
		{
			if (*it)
			{
				if (((*it)->activated()))
				{
					(*it)->deactivate();
				}

				(*it)->activate();

				// add the goal of the child milestone
				switch((*it)->type()) {
					case rxController::eControlType_Joint:
						{
							double default_max_velocity = 0.30;// in rad/s
							double default_min_time = 5.;
							double time = min_time_;

							// q1 and q2 to interpolate between
							dVector current_q = robot_->q();//parent->getConfiguration();
							dVector desired_q = child->getConfiguration();

							// qd1 and qd2
							dVector current_qd(current_q.size());
							current_qd = robot_->qdot();
							dVector desired_qd(current_q.size());
							desired_qd.zero();

							if (max_velocity_ > 0 || min_time_ <= 0 ) {
								double velocity = max_velocity_;
								if (max_velocity_ <= 0) {
									velocity = default_max_velocity;
									time = default_min_time;
								}
								for (unsigned int i = 0; i < current_q.size(); i++) {
									time = max( fabs((6 * desired_q[i] - 6 * current_q[i]) / (current_qd[i] + desired_qd[i] + 4 * velocity)), time);
								}
							}

							std::cout << "[MotionBehaviour::activate] INFO: Time of trajectory: " << time << std::endl;
							time_to_converge_=time;
							dynamic_cast<rxJointController*>(*it)->addPoint(desired_q, time, false, eInterpolatorType_Cubic);
							break;
						}
					case rxController::eControlType_Displacement:
						{
							double default_max_velocity = 0.20;// in m/s
							double default_min_time = 5.;
							double time = min_time_;

							// q1 and q2 to interpolate between
							std::cout << robot_ << std::endl;
							std::cout << robot_->findBody(_T("EE")) << std::endl;
							dVector current_r = robot_->findBody(_T("EE"))->T().r; //parent->getConfiguration();
							dVector desired_r = child->getConfiguration();

							// qd1 and qd2
							dVector current_rd(current_r.size());
							current_rd.zero();
							dVector desired_rd(current_r.size());
							desired_rd.zero();

							if (max_velocity_ > 0 || min_time_ <= 0 ) {
								double velocity = max_velocity_;
								if (max_velocity_ <= 0) {
									velocity = default_max_velocity;
									time = default_min_time;
								}
								for (unsigned int i = 0; i < current_r.size(); i++) {
									time = max( fabs((6 * desired_r[i] - 6 * current_r[i]) / (current_rd[i] + desired_rd[i] + 4 * velocity)), time);
								}
							}

							std::cout << "[MotionBehaviour::activate] INFO: Time of trajectory: " << time << std::endl;
							time_to_converge_=time;
							dynamic_cast<rxDisplacementController*>(*it)->addPoint(desired_r, time, false, eInterpolatorType_Cubic);
							break;
						}
					case rxController::eControlType_Orientation:
						{
							double time = (min_time_ > 0) ? min_time_ : 10.0;
							dVector goal = child->getConfiguration();
							Rotation goal_rot(goal[3], goal[4], goal[5]);		
							//NOTE: Suposses that the orientation is defined with 
							// goal[3] = roll
							// goal[4] = pitch
							// goal[5] = yaw
							dynamic_cast<rxOrientationController*>(*it)->addPoint(goal_rot, time, false);
							break;
						}
					case rxController::eControlType_HTransform:
						{
							double time = (min_time_ > 0) ? min_time_ : 10.0;
							dVector goal = child->getConfiguration();
							Vector3D goal_3D(goal[0],goal[1],goal[2]);
							Rotation goal_rot(goal[3], goal[4], goal[5]);		//NOTE: Suposses that the orientation is defined with 
							// goal[3] = roll
							// goal[4] = pitch
							// goal[5] = yaw
							dynamic_cast<rxHTransformController*>(*it)->addPoint(HTransform(goal_rot, goal_3D), time, false);
							break;
						}
				}
			}
		}
	}
}

void MotionBehaviour::deactivate()
{
	if (control_set_)
	{
		control_set_->deactivateAllControllers();
	}
}

bool MotionBehaviour::hasConverged() 
{
	//NOTE (Roberto) : How could we check if it has converged? There is no way to check automatically all the controllers through the control set.
	//We wait until the time to converge is exceeded and then we check if the error is small.
	if(time_ > time_to_converge_)
	{

		//change this!!!!!!!!!!!!!!!!
		dVector error = this->control_set_->e();



		//this->robot_->findBody(_T("EE"))->T().r
		//std::list<rxController*> controllers = this->control_set_->getControllers();
		//std::list<rxController*>::iterator it = controllers.begin();
		//dynamic_cast<rxInterpolatedDisplacementComplianceController*>(*it)->
		//dynamic_cast<rxInterpolatedDisplacementComplianceController*>(*it)->r_beta_target()

		//controllers.front()

		for(int i = 0; i < error.size(); ++i)
		{
			//HACK (George) : This is because I couldn't get the error between the current position and the desired position at the END of the trajectory
			if (::std::abs(error[i]) > 0.1 )
			{
				//#ifdef NOT_IN_RT
				std::cout << "Error in " << i << " is to large = " << ::std::abs(error[i]) << std::endl;
				//#endif
				return false;
			}
		}
		//std::cout << "CONVERGED!!!" << std::endl;
		return true;
	}
	return false;
}

dVector MotionBehaviour::getError() const
{
	return control_set_->e();
}

dVector MotionBehaviour::getDesired() const
{
	dVector desired;
	std::list<rxController*> controllers = control_set_->getControllers();
	for(std::list<rxController*>::const_iterator it = controllers.begin(); it != controllers.end(); ++it)
	{
		if (*it)
		{
			if ((*it)->type() == rxController::eControlType_Joint)
			{
				dVector qd = dynamic_cast<rxJointController*>(*it)->qd();
				desired.expand(qd.size(), qd);
			}
		}
	}
	return desired;
}

dVector MotionBehaviour::getDesiredDot() const
{
	dVector desired_dot;
	std::list<rxController*> controllers = control_set_->getControllers();
	for(std::list<rxController*>::const_iterator it = controllers.begin(); it != controllers.end(); ++it)
	{
		if (*it)
		{
			if ((*it)->type() == rxController::eControlType_Joint)
			{
				dVector qdotd = dynamic_cast<rxJointController*>(*it)->qdotd();
				desired_dot.expand(qdotd.size(), qdotd);
			}
		}
	}
	return desired_dot;
}

dVector MotionBehaviour::getErrorDot() const
{
	dVector error_dot;
	std::list<rxController*> controllers = control_set_->getControllers();
	for(std::list<rxController*>::const_iterator it = controllers.begin(); it != controllers.end(); ++it)
	{
		if (*it)
		{
			if ((*it)->type() == rxController::eControlType_Joint)
			{
				dVector edot = dynamic_cast<rxJointController*>(*it)->edot();
				error_dot.expand(edot.size(), edot);
			}
		}
	}
	return error_dot;
}

dVector MotionBehaviour::getCurrentDotReference() const
{
	dVector current_dot_ref;
	std::list<rxController*> controllers = control_set_->getControllers();
	for(std::list<rxController*>::const_iterator it = controllers.begin(); it != controllers.end(); ++it)
	{
		if (*it)
		{
			if ((*it)->type() == rxController::eControlType_Joint)
			{
				dVector qdot_ref = dynamic_cast<rxJointController*>(*it)->qdot_ref();
				current_dot_ref.expand(qdot_ref.size(), qdot_ref);
			}
		}
	}
	return current_dot_ref;
}

dVector MotionBehaviour::getCurrentDotDotReference() const
{
	dVector current_dotdot_ref;
	std::list<rxController*> controllers = control_set_->getControllers();
	for(std::list<rxController*>::const_iterator it = controllers.begin(); it != controllers.end(); ++it)
	{
		if (*it)
		{
			if ((*it)->type() == rxController::eControlType_Joint)
			{
				dVector qddot_ref = dynamic_cast<rxJointController*>(*it)->qddot_ref();
				current_dotdot_ref.expand(qddot_ref.size(), qddot_ref);
			}
		}
	}
	return current_dotdot_ref;
}

dVector MotionBehaviour::update(double t)
{
	dVector torque;
	control_set_->compute(t,torque);
	time_ += dT_;
	return torque;
}

std::string MotionBehaviour::toStringXML() const
{
	TiXmlDocument document_xml;
	TiXmlElement * mb_element = this->toElementXML();	
	document_xml.LinkEndChild(mb_element);

	// Declare a printer
	TiXmlPrinter printer_xml;
	// Attach it to the document you want to convert in to a std::string
	document_xml.Accept(&printer_xml);
	// Create a std::string and copy your document data in to the string
	std::string return_value = printer_xml.CStr();

	//TODO: Memory leaks????????????????
	//delete mb_element;
	return return_value;
}

TiXmlElement* MotionBehaviour::toElementXML() const
{
	TiXmlElement * mb_element = new TiXmlElement("MotionBehaviour");
	TiXmlElement * control_set_element = new TiXmlElement("ControlSet");
	mb_element->LinkEndChild(control_set_element);
	// NOTE: This doesn't work anymore since control_set_ is now rxControlSetBase*
	//std::string control_set_name = std::string(typeid(control_set_).name());
	//std::string control_set_name2 = control_set_name.substr(6);		//Ignore the 'class ' part
	//control_set_name2.resize(control_set_name2.size()-2);	//Ignore the ' *' part
	//control_set_element->SetAttribute("type", control_set_name2.c_str());

	if(dynamic_cast<rxTPOperationalSpaceControlSet*>(control_set_))
	{
		control_set_element->SetAttribute("type", "rxTPOperationalSpaceControlSet");
	}
	else if(dynamic_cast<rxControlSet*>(control_set_))
	{
		control_set_element->SetAttribute("type", "rxControlSet");
	}

	std::list<rxController*> controllers = control_set_->getControllers();
	string_type controllers_to_string;
	for(std::list< rxController* >::const_iterator controllers_it = controllers.begin();
		controllers_it != controllers.end() ; controllers_it ++){
			string_type controller_to_string;
			(*controllers_it)->toString(controller_to_string);
			TiXmlElement * rxController_xml = new TiXmlElement("Controller");
			control_set_element->LinkEndChild(rxController_xml);
			this->RLabInfoString2ElementXML_(controller_to_string, rxController_xml);
			rxController_xml->SetAttribute("goalController", (goal_controllers_.find((*controllers_it)->name())->second ? "true" : "false"));
			dVector controller_goal;
			double controller_goal_value;
			//if(!is_goal_controller)
			//{
			//	std::stringstream controller_goal_ss = std::stringstream(xml_deserializer_.deserializeString("controllerGoal"));
			//	while(controller_goal_ss >> controller_goal_value)
			//	{	
			//		controller_goal.expand(1,controller_goal_value);		
			//	}
			//}
			rxController_xml->SetAttribute("priority", (*controllers_it)->priority());
	}
	return mb_element;
}

void MotionBehaviour::RLabInfoString2ElementXML_(string_type string_data, TiXmlElement* out_xml_element) const
{
#ifdef NOT_IN_RT
	//std::wcout << string_data.c_str() << std::endl;
#endif
	std::wstringstream data_ss(string_data);
	string_type temp_st;
	std::getline(data_ss, temp_st);	
	out_xml_element->SetAttribute("type", wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"))).c_str());

	ControllerType type_of_controller = XMLDeserializer::controller_map_[wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))];

	std::getline(data_ss, temp_st);		// Discard field "name"
	std::getline(data_ss, temp_st);		// Discard field "system"
	std::getline(data_ss, temp_st);		// Discard field "type"
	std::getline(data_ss, temp_st);		// Field "dim"
	int dimension_int = -1;
	std::wstringstream dimension_ss(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")));
	dimension_ss >> dimension_int;
	std::getline(data_ss, temp_st);		// Discard field "dt"
	std::getline(data_ss, temp_st);		// Discard field "activated"
	std::getline(data_ss, temp_st);		// Field "ik"
	out_xml_element->SetAttribute("ik", wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"))).c_str());

	string_type kp_st;
	string_type kv_st;
	string_type invL2sqr_st;
	for(int i=0; i<dimension_int ; i++)
	{
		std::getline(data_ss, temp_st);	
		kp_st += temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"));
		if(i!=dimension_int-1)
			kp_st += L" ";

		std::getline(data_ss, temp_st);	
		kv_st += temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"));
		if(i!=dimension_int-1)
			kv_st += L" ";

		std::getline(data_ss, temp_st);	
		invL2sqr_st += temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"));
		if(i!=dimension_int-1)
			invL2sqr_st += L" ";
	}
	out_xml_element->SetAttribute("kp",wstring2string(kp_st).c_str());
	out_xml_element->SetAttribute("kv",wstring2string(kv_st).c_str());
	out_xml_element->SetAttribute("invL2sqr",wstring2string(invL2sqr_st).c_str());

	std::getline(data_ss, temp_st);			// Field "number of via points"
	int num_via_points_int = -1;
	std::wstringstream num_via_points_ss(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")));
	num_via_points_ss >> num_via_points_int;

	for(int i=0; i< num_via_points_int; i++)
	{
		std::getline(data_ss, temp_st);		// Discard field "time" of via points
		std::getline(data_ss, temp_st);		// Discard field "type" of via points
		std::getline(data_ss, temp_st);		// Discard field "reuse" of via points 
		switch( type_of_controller.first )
		{
		case rxController::eControlType_Joint:
			std::getline(data_ss, temp_st);		// Discard field "dVector" of via points 
			break;
		case rxController::eControlType_Displacement:
			std::getline(data_ss, temp_st);		// Discard field "Vector3D" of via points 
			break;
		case rxController::eControlType_Orientation:
			std::getline(data_ss, temp_st);	
			//via_point_xml->SetAttribute("R", wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"))).c_str());
			break;
		case rxController::eControlType_HTransform:
			std::getline(data_ss, temp_st);	
			//via_point_xml->SetAttribute("R", wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"))).c_str());
			std::getline(data_ss, temp_st);	
			//via_point_xml->SetAttribute("r", wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"))).c_str());
			break;
		default:
			break;
		}
	}

	switch(type_of_controller.first)
	{
	case rxController::eControlType_Displacement:
		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("alpha", colon2space( wstring2string( temp_st.substr( temp_st.find(L"=") + 1, temp_st.find(L"\n") ) ) ).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("alphaDisplacement", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("beta", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("betaDisplacement",colon2space( wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );
		break;
	case rxController::eControlType_Orientation:
		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("alpha", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("alphaRotation", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("beta", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("betaRotation", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );
		break;
	case rxController::eControlType_HTransform:
		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("alpha", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("alphaRotation", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );


		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("alphaDisplacement", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("beta", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("betaRotation", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );

		std::getline(data_ss, temp_st);	
		out_xml_element->SetAttribute("betaDisplacement", colon2space(wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")))).c_str() );
		break;
	default:
		break;
	}

	if(type_of_controller.second & WITH_COMPLIANCE)
	{
		std::string stiffness_b;
		std::string stiffness_k;
		for(int i = 0; i<dimension_int; i++)
		{
			stiffness_b += std::string("15");
			stiffness_k += std::string("100");
			if(i!=dimension_int -1)
			{
				stiffness_b += std::string(" ");
				stiffness_k += std::string(" ");
			}
		}
		out_xml_element->SetAttribute("stiffness_b", stiffness_b.c_str() );
		out_xml_element->SetAttribute("stiffness_k", stiffness_k.c_str() );
	}
}

MotionBehaviour* MotionBehaviour::clone() const 
{
	MotionBehaviour* newMotionBehaviour = new MotionBehaviour(*this);
	return newMotionBehaviour;
}

MotionBehaviour& MotionBehaviour::operator=(const MotionBehaviour & motion_behaviour_assignment)
{
	Edge::operator=(motion_behaviour_assignment);
	this->robot_ = motion_behaviour_assignment.robot_;
	this->time_ = motion_behaviour_assignment.time_;
	this->dT_ = motion_behaviour_assignment.dT_;
	this->goal_controllers_ = motion_behaviour_assignment.goal_controllers_;

	// Delete the current stored control set
	if(this->control_set_)
	{
		delete control_set_;
		control_set_ = NULL;
	}

	// Copy the new control set
	if(motion_behaviour_assignment.control_set_)
	{
		// Now are different types of control set allowed
		if(dynamic_cast<rxTPOperationalSpaceControlSet*>(motion_behaviour_assignment.control_set_))
		{
			this->control_set_ = new rxTPOperationalSpaceControlSet((*dynamic_cast<rxTPOperationalSpaceControlSet*>(motion_behaviour_assignment.control_set_)));
		}
		else if(dynamic_cast<rxControlSet*>(motion_behaviour_assignment.control_set_))
		{
			this->control_set_ = new rxControlSet((*dynamic_cast<rxControlSet*>(motion_behaviour_assignment.control_set_)));
		}
		//this->control_set_ = new rxControlSet(*(motion_behaviour_assignment.control_set_));
		this->control_set_->setGravity(0,0,-GRAV_ACC);
		//this->control_set_->setInverseDynamicsAlgorithm(new rxAMBSGravCompensation(this->robot_));
	}
	return *this;
}



void MotionBehaviour::print()
{
	std::list<rxController*> controllers = control_set_->getControllers();
	string_type controllers_to_string;
	for(std::list< rxController* >::const_iterator controllers_it = controllers.begin();
		controllers_it != controllers.end() ; controllers_it ++){
			string_type controller_to_string;
			(*controllers_it)->toString(controller_to_string);

			std::wcout << controller_to_string << std::endl;

			//		int num_via_points_int = -1;
			//		std::wstringstream num_via_points_ss(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n")));
			//		num_via_points_ss >> num_via_points_int;
			//		for(int i=0; i< num_via_points_int; i++)
			//		{
			//			std::getline(data_ss, temp_st);		// Discard field "time" of via points
			//			std::getline(data_ss, temp_st);		// Discard field "type" of via points
			//			std::getline(data_ss, temp_st);		// Discard field "reuse" of via points 
			//			switch( type_of_controller.first ){
			//case JOINT_SPACE_CONTROLLER:
			//	std::getline(data_ss, temp_st);		// Discard field "dVector" of via points 
			//	break;
			//case rxController::eControlType_Displacement:
			//	std::getline(data_ss, temp_st);		// Discard field "Vector3D" of via points 
			//	break;
			//case rxController::eControlType_Orientation:
			//	std::getline(data_ss, temp_st);	
			//	//via_point_xml->SetAttribute("R", wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"))).c_str());
			//	break;
			//case rxController::eControlType_HTransform:
			//	std::getline(data_ss, temp_st);	
			//	//via_point_xml->SetAttribute("R", wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"))).c_str());
			//	std::getline(data_ss, temp_st);	
			//	//via_point_xml->SetAttribute("r", wstring2string(temp_st.substr(temp_st.find(L"=") + 1, temp_st.find(L"\n"))).c_str());
			//	break;
			//default:
			//	break;
			//			}
			//		}

	}
}

void MotionBehaviour::setMaxVelocityForInterpolation(double max_velocity) {
	max_velocity_ = max_velocity;
}

void MotionBehaviour::setMinTimeForInterpolation(double min_time) {
	min_time_ = min_time;
}
