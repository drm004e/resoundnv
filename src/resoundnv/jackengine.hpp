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

#ifndef __JACKENGINE_H__
#define __JACKENGINE_H__

//#include "buffer.hpp"
#include <jack/jack.h>
#include <string>
#include <list>
#include <cassert>
#include <set>
#include <pthread.h>
#include <iostream>

class JackEngine;

typedef std::list<std::string> JackPortNameList;

/// automagic registration and deregistration of jack ports
class JackPort{
	JackEngine* m_jack;
	jack_port_t* m_port;
	std::string m_name;
	JackPortFlags m_flags;
public:
	JackPort(std::string id, JackPortFlags flags, JackEngine* jack);
	virtual ~JackPort();
	float* get_audio_buffer(jack_nframes_t nframes);
	void connect(std::string portName);
	void disconnect(std::string portName);
	void disconnect_all();
};

/// an abstract base class for using jack in a class
class JackEngine {
	friend class JackPort;
private:
	std::string m_name; ///< the jack client name
	jack_client_t* m_jc; ///< the jack client ptr

	jack_nframes_t m_bufferSize; ///< the current bufferSize
	jack_nframes_t m_sampleRate; ///< the current sample rate

	bool m_dspIsRunning;
public:
	JackEngine();
	virtual ~JackEngine();
	void init(const std::string name);
	void start();
	void stop();
	void close();
	
	virtual void on_init(){}; // after jack has been initialised
	virtual void on_start(){}; // prior to start
	virtual void on_stop(){}; // post stoped
	virtual void on_close(){}; // post closed

	// these are virtualized versions of the callbacks
	virtual int on_buffer_size(jack_nframes_t nframes){  return 0; }
	virtual void on_client_registration(const char* name){ }
	virtual void on_freewheel(int starting){ }
	virtual int on_graph_order(){ return 0; }
	virtual void on_port_registration(jack_port_id_t port){}
	virtual void on_port_connect(jack_port_id_t a, jack_port_id_t b, int connect){}
	virtual int on_process(jack_nframes_t nframes){ return 0;}
	virtual int on_sample_rate(jack_nframes_t nframes){ return 0;}
	virtual int on_thread_init(){return 0;}
	virtual int on_xrun(){return 0;}

	/// return the actual client pointer
	jack_client_t* get_client(){return m_jc;}

	bool dsp_is_running(){return m_dspIsRunning;}
	
	/// return a list of ports
	void get_ports(JackPortNameList& portList,const std::string& portNamePattern, const std::string& typeNamePattern);

	jack_nframes_t get_buffer_size() { return m_bufferSize; }
	jack_nframes_t get_sample_rate() { return m_sampleRate; }
private: 
	/// jack static callbacks
	static int jack_buffer_size_callback(jack_nframes_t nframes, void *arg);
	static void jack_client_registration_callback(const char *name, int register, void *arg);
	static void jack_freewheel_callback(int starting, void *arg);
	static int jack_graph_order_callback(void *arg);
	static void jack_port_registration_callback(jack_port_id_t port, int, void *arg);
	static void jack_port_connect_callback(jack_port_id_t a, jack_port_id_t b, int connect, void *arg);
	static int jack_process_callback(jack_nframes_t nframes, void *arg);
	static int jack_sample_rate_callback(jack_nframes_t nframes, void *arg);
	static void jack_thread_init_callback(void *arg);
	static int jack_xrun_callback(void *arg);
	
};

#endif
