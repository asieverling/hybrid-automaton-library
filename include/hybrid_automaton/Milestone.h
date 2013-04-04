/*
* Milestone.h
*
* TODO should/CAN class be abstract? if yes, which member(s) should be pure virtual?
*
* @see Graph
*
*/

#ifndef MILESTONE_
#define MILESTONE_

#include <iostream>
#include "graph/include/MDPNode.h"
#include "hybrid_automaton/thirdparty/tinyxml/include/tinyxml.h"
#include "rxControlSDK/rxControlSDK.h"


typedef struct Point
{
	double x;
	double y;
	double z;

	Point(double X, double Y, double Z)
	{
		x = X;
		y = Y;
		z = Z;
	}

	bool operator ==(const Point & p) const 
	{
		return (this->x==p.x && this->y==p.y && this->z==p.z );
	}

	bool operator ==(const Point & p) 
	{
		return (this->x==p.x && this->y==p.y && this->z==p.z );
	}


}point;

class Milestone : public MDPNode 
{

public:

	enum Status {
		VALID, INVALID, TASK_CONSISTENT, STATUS_NUM
	};

	Milestone();

	Milestone(std::string milestone_name);

	Milestone(const char* milestone_name);

    Milestone(const Milestone &milestone_cpy);

	virtual ~Milestone();

	Milestone::Status getStatus() const;

	void setStatus(Milestone::Status status);

	virtual void update();

	virtual std::string toStringXML() const;

	virtual TiXmlElement* toElementXML() const;

	virtual Milestone* clone() const;

	virtual bool operator ==(const Milestone & n) const;

	virtual bool operator !=(const Milestone & n) const;

	virtual dVector getConfiguration() const;

	virtual std::string getName() const;

	virtual bool hasConverged(rxSystem* sys);

protected:

	Status			status_;
	std::string		name_;
};

#endif // MILESTONE_
