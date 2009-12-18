#pragma once

#include <cstdlib>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <iostream>

#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <sndfile.h>
#include <pthread.h>

#include "resound_exception.hpp"
#include "dsp.hpp"
#include "math3d.hpp"
#include "jackengine.hpp"
#include "oscmanager.hpp"
#include "xmlhelpers.hpp"

typedef std::string ObjectId;

class ResoundSession;

// all xml based objects base to this
class DynamicObject{
	ObjectId id_;
	ResoundSession* session_;
public:
	DynamicObject(const xmlpp::Node* node, ResoundSession* session);
	virtual ~DynamicObject(){}; 

	const ObjectId& get_id(){return id_;}
	ResoundSession& get_session(){return *session_;}
};

