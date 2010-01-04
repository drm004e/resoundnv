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

#include "resound_types.hpp"
#include "behaviour.hpp"

/// a stream - a wrapper around an available input buffer
class AudioStream : public DynamicObject {
	AudioBuffer buffer_;
	float gain_;
	VUMeter vuMeter_;
public:

	AudioStream(const xmlpp::Node* node, ResoundSession* session);
	/// abstract virtualised dsp call
	/// class is expected to make its next buffer of audio ready.
	virtual void process(jack_nframes_t nframes) = 0;
	// gain setting optional from xml
	float get_gain(){return gain_;}
	AudioBuffer* get_buffer(){return &buffer_;}
	VUMeter& get_vu_meter(){return vuMeter_;}

};

/// a stream - a wrapper around an available input buffer
class Diskstream : public AudioStream {
	jack_ringbuffer_t* ringBuffer_; ///< a ring buffer is used to ensure non-locking thread safe read
	static const size_t DISK_STREAM_RING_BUFFER_SIZE = 4096;
	float* diskBuffer_;
	SNDFILE* file_;
	SF_INFO info_;
	std::string path_;
	bool playing_;
public:

	/// construct
	Diskstream(const xmlpp::Node* node, ResoundSession* session);

	/// destruct
	virtual ~Diskstream();

	/// class is expected to pull as much data as it can from disk into the ring buffer
	/// this is called from a disk reading thread
	virtual void disk_process();

	/// class is expected to make its next buffer of audio ready. read a block from the ringbuffer
	virtual void process(jack_nframes_t nframes);

	/// method to seek the current disk location, lock thread mutex first!
	void seek(size_t pos);

	/// start this stream, lock disk thread mutex first!
	void play();
	/// stop this stream, lock disk thread mutex first!
	void stop();
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
	Vec3 pos_;
	float az_, el_;
	float gain_;
protected:
	VUMeter vuMeter_;
public:
	Loudspeaker(const xmlpp::Node* node, ResoundSession* session);
	/// class is expected to make its next buffer of audio ready to be written too.
	virtual void pre_process(jack_nframes_t nframes);
	/// buffer should have audio from behaviours in it.. post-process onto jack stream
	virtual void post_process(jack_nframes_t nframes);
	// return the buffer so the objects may write into it
	AudioBuffer* get_buffer(){return &buffer_;}
	/// return the speakers position relative to the origin.
	const Vec3& get_position() const {return pos_;}
	/// return the vumetering object
	VUMeter& get_vu_meter(){return vuMeter_;}
};


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

	typedef std::vector<Diskstream*> DiskstreamVector;
	DiskstreamVector diskStreams_;

	typedef std::vector<Loudspeaker*> LoudspeakerVector;
	LoudspeakerVector loudspeakers_;

	typedef std::vector<Behaviour*> BehaviourVector;
	BehaviourVector behaviours_;

	/// map of behaviour factories by plugin name
	typedef std::map<ObjectId,BehaviourFactory> BehaviourFactoryMap;
	BehaviourFactoryMap behaviourFactories_;

	/// the thread id for the diskstream loading thread
	pthread_t diskstreamThreadId_;
	pthread_mutex_t diskstreamThreadLock_;
	pthread_cond_t  diskstreamThreadReady_;
        bool diskstreamThreadContinue_;

public:
	/// construct a new session from the xml file specified
	ResoundSession(CLIOptions options);

        /// destructor
        ~ResoundSession();
private:
	/// osc callback static
	static int lo_play(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int lo_stop(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int lo_seek(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);

	/// diskstream play
	void diskstream_play();
	/// diskstream stop
	void diskstream_stop();
	/// diskstream seek
	void diskstream_seek(size_t pos);
public:
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

	/// resolve an id to an actual audio stream
	AudioStream* resolve_audiostream(ObjectId id);

	/// resolve an id to an actual audio stream
	Loudspeaker* resolve_loudspeaker(ObjectId id);

	/// builds the fast index tables of various dsp related objects
	void build_dsp_object_lookups();

	/// jack dsp callback
	virtual int on_process(jack_nframes_t nframes);
	
	/// send osc relating to regular feedback to any listening clients
	/// this should be called periodicaly by a thread
	/// sends vu metering osc for each loudspeaker and audiostream.
	void send_osc_feedback();



private:
	/// check disk input buffers are full and cause a load if they are not
	/// called by the disk input thread, syncronised by the process thread.
	void diskstream_process();
	static void* diskstream_thread (void *arg);
};
