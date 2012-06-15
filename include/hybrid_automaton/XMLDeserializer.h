#ifndef XML_DESERIALIZER
#define XML_DESERIALIZER

#include <string>
#include <map>
#include "tinyxml.h"

#include "HybridAutomaton.h"
#include "MotionBehaviour.h"
#include "Milestone.h"
#include "CSpaceMilestone.h"
#include "OpSpaceMilestone.h"

template<class T>
T deserializeElement(TiXmlElement * xml_element, const char * field_name);

template<class T>
T deserializeElement(TiXmlElement * xml_element, const char * field_name, T default_value);

bool deserializeBoolean(TiXmlElement * xml_element, const char * field_name, bool default_value = false);

std::string deserializeString(TiXmlElement * xml_element, const char * field_name);

std::vector<double> deserializeVectorDouble(TiXmlElement * xml_element, const char * field_name);

ViaPointBase * deserializeViaPoint(TiXmlElement * xml_element, ControllerType type_of_controller, int controller_dimension);

/**
* Convert a wstring into a string.
* @param wstr Wide string to be converted.
*/
std::string wstring2string(const std::wstring& wstr);

/**
* Convert a string into a wstring.
* @param str String to be converted.
*/
std::wstring string2wstring(const std::string& str);

/**
* Replace the colons of a string with white spaces.
* @param text String to be processed.
*/
std::string colon2space(std::string text);


class XMLDeserializer
{
public:
	XMLDeserializer();
	virtual ~XMLDeserializer();

	static HybridAutomaton* createHybridAutomaton(const std::string& xml_string, rxSystem* robot, double dT);

	static CSpaceMilestone* createCSpaceMilestone(TiXmlElement* milestone_xml, rxSystem* robot, double dT);

	static OpSpaceMilestone* createOpSpaceMilestone(TiXmlElement* milestone_xml, rxSystem* robot, double dT);

	static MotionBehaviour* createMotionBehaviour(TiXmlElement* motion_behaviour_xml , const Milestone *dad, const Milestone *son , rxSystem* robot, double dT );

	static rxController* createController(TiXmlElement* rxController_xml , const Milestone *dad, const Milestone *son , rxSystem* robot, double dT, 
		bool goal_controller, int controller_counter);

	/**
	* Recreate a Joint controller
	* @param joint_subgroup Subgroup of the controller to be created within the Joint Controller group.
	* @param controller_duration Time interval of the controller to be created.
	* @param via_points_ptr Via points of the controller to be created.
	*/
	static rxController* createJointController(int joint_subtype, double controller_duration, std::vector<ViaPointBase*> via_points_ptr, 
		TiXmlElement* rxController_xml, rxSystem* robot);

	/**
	* Recreate a Displacement controller
	* @param displacement_subgroup Subgroup of the controller to be created within the Displacement Controller group.
	* @param controller_duration Time interval of the controller to be created.
	* @param via_points_ptr Via points of the controller to be created.
	* @param rxController_xml TinyXML Element with some other required information (alpha, alpha displacement, beta, beta displacement).
	*/
	static rxController* createDisplacementController(int displacement_subtype, double controller_duration, std::vector<ViaPointBase*> via_points_ptr, 
		TiXmlElement* rxController_xml, rxSystem* robot);

	/**
	* Recreate an Orientation controller
	* @param orientation_subgroup Subgroup of the controller to be created within the Orientation Controller group.
	* @param controller_duration Time interval of the controller to be created.
	* @param via_points_ptr Via points of the controller to be created.
	* @param rxController_xml TinyXML Element with some other required information (alpha, alpha orientation, beta, beta orientation).
	*/
	static rxController* createOrientationController(int orientation_subtype, double controller_duration, std::vector<ViaPointBase*> via_points_ptr, 
		TiXmlElement* rxController_xml, rxSystem* robot);

	/**
	* Recreate a HTransform controller
	* @param htransform_subgroup Subgroup of the controller to be created within the HTransform Controller group.
	* @param controller_duration Time interval of the controller to be created.
	* @param via_points_ptr Via points of the controller to be created.
	* @param rxController_xml TinyXML Element with some other required information (alpha, alpha htransform, beta, beta htransform).
	*/
	static rxController* createHTransformController(int htransform_subtype, double controller_duration, std::vector<ViaPointBase*> via_points_ptr, 
		TiXmlElement* rxController_xml, rxSystem* robot);

	/**
	* Recreate a QuasiCoord controller
	* @param quasi_coord_subgroup Subgroup of the controller to be created within the Joint Controller group.
	* @param controller_duration Time interval of the controller to be created.
	*/
	static rxController* createQuasiCoordController(int quasi_coord_subtype, double controller_duration, rxSystem* robot);

	/**
	* Recreate a NullMotion controller
	* @param null_motion_coord_subgroup Subgroup of the controller to be created within the Null Motion Controller group.
	* @param controller_duration Time interval of the controller to be created.
	*/
	static rxController* createNullMotionController(int null_motion_subtype, double controller_duration, rxSystem* robot);

	/**
	* Recreate a Functional controller
	* @param functional_subtype Subgroup of the controller to be created within the Null Motion Controller group.
	* @param dimension Dimension to be controlled
	* @param controller_duration Time interval of the controller to be created.
	*/
	static rxController* createFunctionalController(int functional_subtype, int dimension, double controller_duration, TiXmlElement* rxController_xml,
		rxSystem* robot);

	static std::map<std::string, ControllerType> createControllerMapping();

	static std::map<std::string, ControllerType>	controller_map_;	// Translation between class name (string) of the controllers and their type
};

#endif	//XML_DESERIALIZER