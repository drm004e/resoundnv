#pragma once

#include <cstdlib>
#include <string>
#include <sstream>
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

/// a reference to a buffer
struct BufferRef{
public:
    ObjectId id;
    AudioBuffer* buffer;
    bool isBus;
    bool isAlias;
    ObjectId creator;
};
typedef std::map<ObjectId,BufferRef> BufferRefMap;
typedef std::vector<BufferRef> BufferRefVector;

class ResoundSession;

// all xml based objects base to this
class DynamicObject{
	ObjectId id_;
public:
	DynamicObject();
	virtual void init_from_xml(const xmlpp::Element* nodeElement);
	virtual ~DynamicObject(){}; 
	const ObjectId& get_id(){return id_;}
};

class ResoundApp {
private:
	ResoundApp();
	~ResoundApp();
	static ResoundApp* s_singleton;
	ResoundSession* m_session;
public:
	static ResoundApp& get_instance();
	ResoundSession& get_session(){return *m_session;}
	void set_session(ResoundSession* session){m_session = session;}
};
#define APP() ResoundApp::get_instance()
#define SESSION() ResoundApp::get_instance().get_session()

