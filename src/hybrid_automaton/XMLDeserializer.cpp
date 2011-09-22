#include "XMLDeserializer.h"
#include "rControlalgorithm\rControlalgorithm.h"
#include "rxControlSDK\rxControlSDK.h"

XMLDeserializer::XMLDeserializer(TiXmlElement * xml_element_in):
xml_element(xml_element_in)
{
}

XMLDeserializer::~XMLDeserializer()
{
}


bool XMLDeserializer::deserializeBoolean(const char * field_name)
{
	bool return_boolean;
	std::string string_bool = std::string(xml_element->Attribute(field_name));
	if(string_bool == std::string("true"))
	{
		return_boolean = true;
	}
	else if(string_bool == std::string("false"))
	{
		return_boolean = false;
	}
	else
	{
		std::string exception_str = std::string("ERROR: [XMLDeserializer::deserializeBoolean(const char * field_name)] Attribute ") + std::string(field_name) + std::string(" was not found in XML element.");
		throw exception_str;
	}
	return return_boolean;
}

int XMLDeserializer::deserializeInteger(const char * field_name)
{
	int return_integer = -1;
	if(!xml_element->Attribute(field_name, &return_integer))
	{
		std::string exception_str = std::string("ERROR: [XMLDeserializer::deserializeInteger(const char * field_name)] Attribute ") + std::string(field_name) + std::string(" was not found in XML element.");
		throw exception_str;
	}
	return return_integer;
}

double XMLDeserializer::deserializeDouble(const char * field_name)
{
	double return_double = -1.0;
	if(!xml_element->Attribute(field_name, &return_double))
	{
		std::string exception_str = std::string("ERROR: [XMLDeserializer::deserializeDouble(const char * field_name)] Attribute ") + std::string(field_name) + std::string(" was not found in XML element.");
		throw exception_str;
	}
	return return_double;
}

std::string XMLDeserializer::deserializeString(const char * field_name)
{
	std::string return_string = std::string(xml_element->Attribute(field_name));
	if(return_string == std::string(""))
	{
		std::string exception_str = std::string("ERROR: [XMLDeserializer::deserializeString(const char * field_name)] Attribute ") + std::string(field_name) + std::string(" was not found in XML element.");
		throw exception_str;
	}
	return return_string;
}

ViaPointBase * XMLDeserializer::deserializeViaPoint(ControllerType type_of_controller, int controller_dimension)
{
	ViaPointBase * return_value;
	double via_point_time = this->deserializeDouble("time");
	int via_point_type = this->deserializeInteger("type");
	bool via_point_reuse_bool = this->deserializeBoolean("reuse");
	switch( type_of_controller.first )
	{
	case JOINT_SPACE_GROUP:	
		{
		std::stringstream via_point_ss = std::stringstream(this->deserializeString("dVector"));
		double via_point_value = -1.0;
		dVector via_point_dVector;
		for(int i=0; i<controller_dimension; i++)
		{
			via_point_ss >> via_point_value;
			via_point_dVector.expand(1,via_point_value);
		}
		return_value = new ViaPointdVector(via_point_time, via_point_type, via_point_reuse_bool, via_point_dVector);
		break;
		}
	case DISPLACEMENT_GROUP:
		{
		std::stringstream via_point_ss = std::stringstream(this->deserializeString("Vector3D"));
		double via_point_value = -1.0;
		Vector3D via_point_Vector3D;
		for(int i=0; i<controller_dimension; i++)
		{
			via_point_ss >> via_point_value;
			via_point_Vector3D.expand(1,via_point_value);
		}
		return_value = new ViaPointVector3D(via_point_time, via_point_type, via_point_reuse_bool, via_point_Vector3D);
		break;
		}
	case ORIENTATION_GROUP:
		{
		std::stringstream via_point_ss = std::stringstream(this->deserializeString("R"));
		double via_point_value = -1.0;
		std::vector<double> via_point_values;
		for(int i=0; i<9; i++)
		{
			via_point_ss >> via_point_value;
			via_point_values.push_back(via_point_value);
		}
		Rotation via_point_Rotation(via_point_values[0], via_point_values[1],via_point_values[2],via_point_values[3],via_point_values[4],via_point_values[5],via_point_values[6],via_point_values[7],via_point_values[8]);
		return_value = new ViaPointRotation(via_point_time, via_point_type, via_point_reuse_bool, via_point_Rotation);
		break;
		}
	case H_TRANSFORM_GROUP:
		{
		std::stringstream via_point_ss = std::stringstream(this->deserializeString("R"));
		double via_point_value = -1.0;
		std::vector<double> via_point_values;
		for(int i=0; i<9; i++)
		{
			via_point_ss >> via_point_value;
			via_point_values.push_back(via_point_value);
		}
		Rotation via_point_Rotation(via_point_values[0], via_point_values[1],via_point_values[2],via_point_values[3],via_point_values[4],via_point_values[5],via_point_values[6],via_point_values[7],via_point_values[8]);

		std::stringstream via_point_ss2 = std::stringstream(this->deserializeString("r"));
		via_point_values.clear();
		for(int i=0; i<9; i++)
		{
			via_point_ss2 >> via_point_value;
			via_point_values.push_back(via_point_value);
		}
		Displacement via_point_Displacement(via_point_values[0], via_point_values[1],via_point_values[2]);		

		HTransform via_point_HTransform(via_point_Rotation, via_point_Displacement);

		return_value = new ViaPointHTransform(via_point_time, via_point_type, via_point_reuse_bool, via_point_HTransform);
		break;
		}
	default:
		std::string exception_str = std::string("ERROR: [XMLDeserializer::deserializeViaPoint(ControllerType type_of_controller, int controller_dimension)] Unknown type of Controller group.");
		throw exception_str;
		break;
	}
	return return_value;
}