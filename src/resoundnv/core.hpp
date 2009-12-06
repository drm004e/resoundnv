//    Resound
//    Copyright 2009 David Moore and James Mooney
//
//    This file is part of Resound.
//
//    Resound is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    Resound is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Resound; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#pragma once

#include <cstdlib>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <iostream>
#include <exception>

#include "jackengine.hpp"
#include "oscmanager.hpp"


#include <libxml++/libxml++.h>

class Exception : public std::exception {
	const char* msg_;
public:
	Exception(const char* msg) : msg_(msg) {}
	const char* what() const throw() { return msg_; }
};

class ResoundSession;

typedef std::string ObjectId;

// xml parsing helpers
const xmlpp::Element* get_element(const xmlpp::Node* node);
std::string get_attribute_string(const xmlpp::Element* node, const std::string& name);
std::string get_optional_attribute_string(const xmlpp::Element* node, const std::string& name, std::string def);
float get_optional_attribute_float(const xmlpp::Element* node, const std::string& name, float def);

/// memory managed audio buffer
class AudioBuffer {
	size_t size_;
	float* buffer_;
public:
	AudioBuffer();
	~AudioBuffer();
	void allocate( size_t size );
	void clear();
	void destroy();
	float* get_buffer(){return buffer_;}
	//float& operator [](unsigned int n){return buffer_[n];};
	//float* operator *

};

void ab_copy(const float* src, float* dest, size_t N );
void ab_copy_with_gain(const float* src, float* dest, size_t N, float gain);
void ab_sum_with_gain(const float* src, float* dest, size_t N, float gain);

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

// an actual dsp route, created by parsing the routing cass/cls "language"
class BRoute{
	AudioBuffer* fromBuffer_;
	AudioBuffer* toBuffer_;
	float gain_;
public:
	BRoute() : fromBuffer_(0), toBuffer_(0), gain_(1.0f) {}
	BRoute(AudioBuffer* fromBuffer, AudioBuffer* toBuffer, float gain) :
			fromBuffer_(fromBuffer), toBuffer_(toBuffer) , gain_(gain) 
			{}
	AudioBuffer* get_from() const {return fromBuffer_;}
	AudioBuffer* get_to() const {return toBuffer_;}
	float get_gain() const {return gain_; }
};

typedef std::vector<BRoute> BRouteArray; // sequential array of routes

// routes are stored in behaviours
class BRouteSet{

private:
	BRouteArray routes_;
public:
	BRouteSet(const xmlpp::Node* node, ResoundSession* session);
	const BRouteArray& get_routes() const {return routes_; }
};

// parameters for behaviours
class BParam{
	ObjectId id_;
	float value_;
	std::string addr_;
public:
	BParam(const xmlpp::Node* node, ResoundSession* session);
	float get_value(){ return value_; }
	ObjectId get_id(){return id_;}
	ObjectId get_address(){return addr_;}
	// callback for osc
	static int lo_cb_params(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
};

/// A dsp pluginable object abstract class generated by factory
class Behaviour : public DynamicObject {
public:
	typedef std::map<ObjectId,BParam*> BParamMap;
private:
	BParamMap params_;
public:
	Behaviour(const xmlpp::Node* node, ResoundSession* session);
	/// abstract virtualised dsp call
	/// class is expected to copy from input stream buffers to loudspeaker buffers.
	/// some processing would occur on the way
	virtual void process(jack_nframes_t nframes) = 0;

	/// obtain a parameter value
	// TODO this should really be some sort of fast lookup table pre-built at the start of dsp.
	float get_parameter_value(const char* name){ return params_[name]->get_value(); }
};


/// A dsp pluginable object abstract class generated by factory
/// this version is extended to deal with routeset interpretation
/// route are created automatically and the behaviour dsp can access these easily
/// this version should be used as a base for certain behaviours that work with CASS and CLS sets 
class RouteSetBehaviour : public Behaviour {
public:
	typedef std::vector<BRouteSet*> BRouteSetArray; // sequential array of routes
private:
	BRouteSetArray routeSets_;
public:
	RouteSetBehaviour(const xmlpp::Node* node, ResoundSession* session);
	virtual void process(jack_nframes_t nframes) = 0;
	const BRouteSetArray& get_route_sets() const {return routeSets_;}
};

/// an IOBehaviour does not use the routeset interpretation and instead suggests inputs and outputs
/// routing must be entirely handled by the behaviour dsp.
class IOBehaviour : public Behaviour {
public:
	IOBehaviour(const xmlpp::Node* node, ResoundSession* session);
	virtual void process(jack_nframes_t nframes) = 0;
};

/// A dsp pluginable object abstract class generated by factory
class MinimalRouteSetBehaviour : public RouteSetBehaviour {
public:

	MinimalRouteSetBehaviour(const xmlpp::Node* node, ResoundSession* session) : RouteSetBehaviour(node,session){	
		std::cout << "Created minimal routeset behaviour object!" << std::endl;
	}
	virtual void process(jack_nframes_t nframes){};
	static Behaviour* factory(const xmlpp::Node* node, ResoundSession* session) { return new MinimalRouteSetBehaviour(node,session); }
};

/// A dsp pluginable object abstract class generated by factory
class AttBehaviour : public RouteSetBehaviour {
public:

	AttBehaviour(const xmlpp::Node* node, ResoundSession* session) : RouteSetBehaviour(node,session){	
		std::cout << "Created Att routeset behaviour object!" << std::endl;
	}
	virtual void process(jack_nframes_t nframes);
	static Behaviour* factory(const xmlpp::Node* node, ResoundSession* session) { return new AttBehaviour(node,session); }
};

/// A dsp pluginable object abstract class generated by factory
class MinimalIOBehaviour : public IOBehaviour {
public:

	MinimalIOBehaviour(const xmlpp::Node* node, ResoundSession* session) : IOBehaviour(node,session){	
		std::cout << "Created minimal io based behaviour object!" << std::endl;
	}
	virtual void process(jack_nframes_t nframes){};
	static Behaviour* factory(const xmlpp::Node* node, ResoundSession* session) { return new MinimalIOBehaviour(node,session); }
};

/// a stream - a wrapper around an available input buffer
class AudioStream : public DynamicObject {
	AudioBuffer buffer_;
	float gain_;
public:

	AudioStream(const xmlpp::Node* node, ResoundSession* session);
	/// abstract virtualised dsp call
	/// class is expected to make its next buffer of audio ready.
	virtual void process(jack_nframes_t nframes) = 0;
	// gain setting optional from xml
	float get_gain(){return gain_;}
	AudioBuffer* get_buffer(){return &buffer_;}

};

/// a stream - a wrapper around an available input buffer
class Diskstream : public AudioStream {
public:

	Diskstream(const xmlpp::Node* node, ResoundSession* session);

	/// class is expected to make its next buffer of audio ready.
	virtual void process(jack_nframes_t nframes){};
};

/// a stream - a wrapper around an available input buffer
class Livestream : public AudioStream{
	JackPort* port_;
	std::string connectionName_;
public:

	Livestream(const xmlpp::Node* node, ResoundSession* session);

	/// class is expected to make its next buffer of audio ready.
	virtual void process(jack_nframes_t nframes);
};

// aliases are used to store information from cass and cls alias nodes
class Alias{
	ObjectId id_;
	ObjectId ref_;
	float gain_;
public:
	Alias(const xmlpp::Node* node);
	ObjectId get_id(){return id_;}
	ObjectId get_ref(){return ref_;}
	float get_gain(){return gain_;}
};



/// map of Aliases by id, populated by stream managing objects
typedef std::map<ObjectId,Alias*> AliasMap;

// cass and cls are both alias set objects
class AliasSet : public DynamicObject {
	AliasMap aliases_;
public:
	AliasSet(const xmlpp::Node* node, ResoundSession* session);
	/// return an alias by id
	Alias* get_alias(ObjectId id);
	/// get a reference to the aliases list
	const AliasMap& get_aliases() const { return aliases_; }

};
/// a Coherent Audio Stream Set
class CASS : public AliasSet {
public:
	CASS(const xmlpp::Node* node, ResoundSession* session);

};

/// a Coherent Loudspeaker Set
class CLS : public AliasSet {
public:
	CLS(const xmlpp::Node* node, ResoundSession* session);

};

/// a loudspeaker object - a wrapper around an available output buffer
/// contains loudspeaker dsp
class Loudspeaker : public DynamicObject{
	AudioBuffer buffer_;
	JackPort* port_;
	// where does this port connect to initially
	std::string connectionName_;
	// loudspeaker type class - may be used to default settings
	std::string type_;
	/// speaker position attributes
	float x_, y_, z_, az_, el_;
	float gain_;
public:
	Loudspeaker(const xmlpp::Node* node, ResoundSession* session);
	/// class is expected to make its next buffer of audio ready to be written too.
	virtual void pre_process(jack_nframes_t nframes);
	/// buffer should have audio from behaviours in it.. post-process onto jack stream
	virtual void post_process(jack_nframes_t nframes);
	// return the buffer so the objects may write into it
	AudioBuffer* get_buffer(){return &buffer_;}
};

// dsp chain psuedo
//void dsp{
	// call streams::process()
	// call loudspeakers::pre_process()
	// call behaviours::process()
	// call loudspeakers::post_process()
//}

/// behaviours are created by factory function as follows
typedef Behaviour* (*BehaviourFactory)(const xmlpp::Node* node, ResoundSession*);

struct CLIOptions{
	std::string inputXML_;
	std::string oscPort_;
};

/// a resound session will read a single xml file and register all jack and disk streams
class ResoundSession : public JackEngine, public Resound::OSCManager{
private:
	/// options specified
	CLIOptions options_;

	/// a map of all dynamic objects, used as an index to keep all names unique
	/// also enables rtti from any of the dynamic objects
	typedef std::map<ObjectId,DynamicObject*> DynamicObjectMap;
	DynamicObjectMap dynamicObjects_;

	/// a set of fast lookup index tables
	typedef std::vector<AudioStream*> AudioStreamVector;
	AudioStreamVector audioStreams_;

	typedef std::vector<Loudspeaker*> LoudspeakerVector;
	LoudspeakerVector loudspeakers_;

	typedef std::vector<Behaviour*> BehaviourVector;
	BehaviourVector behaviours_;

	/// map of behaviour factories by plugin name
	typedef std::map<ObjectId,BehaviourFactory> BehaviourFactoryMap;
	BehaviourFactoryMap behaviourFactories_;

public:
	/// construct a new session from the xml file specified
	ResoundSession(CLIOptions options);

	/// reset everything and load from xml - ideally we do this with a new session object
	void load_from_xml(const xmlpp::Node* node);

	/// create a behaviour by xml node
	Behaviour* create_behaviour_from_node(const xmlpp::Node* node);

	/// method to register a new behaviour type, used by plugins to register new behaviour classes
	void register_behaviour_factory(std::string name, BehaviourFactory factory);

	/// register a dynamic object by id
	void register_dynamic_object(ObjectId id, DynamicObject* ob);

	/// get a dynamic object by id, this will take care of alias id's returning the cass or cls only
	/// e.g. 'ob1.L' would discard the '.L' and return the object 'ob1'
	/// the method will throw if not found 
	DynamicObject* get_dynamic_object(ObjectId id);

	/// builds the fast index tables of various dsp related objects
	void build_dsp_object_lookups();

	/// jack dsp callback
	virtual int on_process(jack_nframes_t nframes);

};
