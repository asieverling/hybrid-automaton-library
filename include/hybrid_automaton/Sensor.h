#ifndef HYBRID_AUTOMATON_SENSOR_H
#define HYBRID_AUTOMATON_SENSOR_H

#include "hybrid_automaton/Serializable.h"
#include "hybrid_automaton/error_handling.h"
#include "hybrid_automaton/System.h"
#include "hybrid_automaton/HybridAutomatonRegistration.h"

#include <boost/shared_ptr.hpp>

namespace ha {

	class Sensor;
	typedef boost::shared_ptr<Sensor> SensorPtr;
	typedef boost::shared_ptr<const Sensor> SensorConstPtr;

	class Sensor : public Serializable
	{
	public:

		typedef boost::shared_ptr<Sensor> Ptr;
		typedef boost::shared_ptr<const Sensor> ConstPtr;

		Sensor();

		virtual ~Sensor();

		/**
		* Copy constructor
		*/
		Sensor(const Sensor& ss);

		SensorPtr clone() const
		{
			return (SensorPtr(_doClone()));
		};

		virtual ::Eigen::MatrixXd getCurrentValue() const = 0;

		virtual const std::string getType() const;
		virtual void setType(const std::string& new_type);

		virtual void setSystem(const System::ConstPtr& system);

	protected:
		System::ConstPtr _system;

		virtual Sensor* _doClone() const = 0;

		std::string _type;

	};

}

#endif