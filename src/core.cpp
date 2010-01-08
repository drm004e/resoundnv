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

#include "resoundnv/core.hpp"
#include <typeinfo>
#include <cstring>

#include <cmath>

#include <boost/program_options.hpp>




DynamicObject::DynamicObject(){
}

void DynamicObject::init_from_xml(const xmlpp::Element* nodeElement){
	assert(nodeElement);
	std::string name = nodeElement->get_name();
	id_ = get_attribute_string(nodeElement,"id");
	// attempt to register
	SESSION().register_dynamic_object(id_, this);
}

ResoundApp* ResoundApp::s_singleton = 0; 
ResoundApp& ResoundApp::get_instance(){
	if(!s_singleton) s_singleton = new ResoundApp;
	return *s_singleton;
}
ResoundApp::ResoundApp(){
	m_session = 0;
}
ResoundApp::~ResoundApp(){
}



Loudspeaker::Loudspeaker(){}

void Loudspeaker::init_from_xml(const xmlpp::Element* nodeElement){
	// this node will need to establish a jack stream via the jack system, it should kill the port when done

	ObjectId id = get_attribute_string(nodeElement,"id");
	connectionName_ = get_attribute_string(nodeElement,"port");
	port_ = new JackPort(id, JackPortIsOutput ,&SESSION());
	port_->connect(connectionName_);

	type_ = get_optional_attribute_string(nodeElement,"type");
	pos_.x = get_optional_attribute_float(nodeElement,"x");
	pos_.y = get_optional_attribute_float(nodeElement,"y");
	pos_.z = get_optional_attribute_float(nodeElement,"z");
	az_ = get_optional_attribute_float(nodeElement,"az");
	el_ = get_optional_attribute_float(nodeElement,"el");
	
	gain_ = get_optional_attribute_float(nodeElement,"gain",1.0);

	std::cout << "Loudspeaker " << id << " type=" << type_ << std::endl;

	jack_nframes_t s = SESSION().get_buffer_size();
	buffer_.allocate(s);

        // register the buffer
        BufferRef ref;
        std::stringstream str;
        str << "bus." << id;
        ref.id =  str.str();
        ref.isAlias = false;
        ref.isBus = true;
        ref.creator = id;
        ref.buffer = &buffer_;
        SESSION().register_buffer(ref);

	DynamicObject::init_from_xml(nodeElement);
}


void Loudspeaker::pre_process(jack_nframes_t nframes){
	// class is expected to make its next buffer of audio ready to be written too.
	// clear buffer ready for summing.
	buffer_.clear();
	//std::cout << "Loudspeaker::pre_process" << std::endl;
}

void Loudspeaker::post_process(jack_nframes_t nframes){
	// buffer should have audio from behaviours in it.. post-process onto jack stream
	// copy from internal buffer to jack buffer, apply gain during copy

	//avg_signal_in_buffer(buffer_.get_buffer(),nframes); // tested // no sound here

	ab_copy_with_gain(buffer_.get_buffer(), port_->get_audio_buffer(nframes), nframes, gain_);
	//std::cout << "Loudspeaker::post_process" << std::endl;

	//TODO optional vumetering
	vuMeter_.analyse_buffer(buffer_.get_buffer(),nframes);
}

Alias::Alias(const xmlpp::Node* node, ObjectId parent){
	const xmlpp::Element* nodeElement = get_element(node);
	id_= get_attribute_string(nodeElement,"id");
	ref_= get_attribute_string(nodeElement,"ref");

        BufferRefVector v = SESSION().lookup_buffer(ref_);
        if(v.size() == 1){
            BufferRef bref = v[0];
            bref.id = parent + std::string(".") + id_;
            bref.isAlias = true;
            bref.creator = parent ;

            SESSION().register_buffer(bref);
        } else {
            throw Exception("Alias could not find a reference to the buffer requested.");
        }
        
}

AliasSet::AliasSet()
{}
void AliasSet::init_from_xml(const xmlpp::Element* nodeElement){

        ObjectId id = get_attribute_string(nodeElement,"id");
	// look for aliases
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="alias"){
				Alias* alias = new Alias(child,id);
				ObjectId id = alias->get_id();
				AliasMap::iterator it = aliases_.find(id);
				if(it == aliases_.end()){
					aliases_[id] = alias;
					std::cout << "Alias found " << id << std::endl;
				} else {
					throw Exception("non-unique name for alias within AliasSet");
				}	
			}
		}	
	}
//	for( AliasMap::iterator it = aliases_.begin(); it != aliases_.end(); ++it){ }

	DynamicObject::init_from_xml(nodeElement);
}

Alias* AliasSet::get_alias(ObjectId id){
	AliasMap::iterator it = aliases_.find(id);
	if(it != aliases_.end()){
		return it->second;
	} else {
		throw Exception((std::string("Alias not found : ") + id).c_str());
	}
}

ResoundSession::ResoundSession(CLIOptions options) : 
		Resound::OSCManager(options.oscPort_.c_str()),
		options_(options) {

	// setup ladspa hosting
	ladspaHost = new LadspaHost();

	// diskstream threads
	pthread_mutex_init (&diskstreamThreadLock_, NULL);
	pthread_cond_init(&diskstreamThreadReady_, NULL);

	// registering some factories
        register_behaviour_factory("diskstream", Diskstream::factory);
	register_behaviour_factory("livestream", Livestream::factory);
	register_behaviour_factory("att", AttBehaviour::factory);
	register_behaviour_factory("mpc", MultipointCrossfadeBehaviour::factory);
	register_behaviour_factory("chase", ChaseBehaviour::factory);
	register_behaviour_factory("amppan", AmpPanBehaviour::factory);
        register_behaviour_factory("gain", GainInsertBehaviour::factory);
        register_behaviour_factory("ringmod", RingmodInsertBehaviour::factory);
        register_behaviour_factory("ladspa", LADSPABehaviour::factory);

	init("resoundnv-session");

	///activate jack ports cannot be connected until active
	start();

	// register transport osc parameters
	add_method("/resound/t1/play","i", ResoundSession::lo_play, this);
	add_method("/resound/t1/stop","i", ResoundSession::lo_stop, this);
	add_method("/resound/t1/seek","i", ResoundSession::lo_seek, this);
}

int ResoundSession::lo_play(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data){
	ResoundSession* session = static_cast<ResoundSession*>(user_data);
	session->diskstream_play(); 
	std::cout << "Playing\n";
    return 1;
}
int ResoundSession::lo_stop(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data){
	ResoundSession* session = static_cast<ResoundSession*>(user_data);
	session->diskstream_stop(); 
	std::cout << "Stopping\n";
    return 1;
}
int ResoundSession::lo_seek(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data){
	ResoundSession* session = static_cast<ResoundSession*>(user_data);
	session->diskstream_seek(argv[0]->i); // TODO validation here required
	std::cout << "Seeking\n";
    return 1;
}

/// diskstream play
void ResoundSession::diskstream_play(){
	for(unsigned int n = 0; n < diskStreams_.size(); ++n){
		diskStreams_[n]->play();
	}
}
/// diskstream stop
void ResoundSession::diskstream_stop(){
	for(unsigned int n = 0; n < diskStreams_.size(); ++n){
		diskStreams_[n]->stop();
	}
}
/// diskstream seek
void ResoundSession::diskstream_seek(size_t pos){
	for(unsigned int n = 0; n < diskStreams_.size(); ++n){
		diskStreams_[n]->seek(pos);
	}
}


ResoundSession::~ResoundSession(){
    printf("Session is closing...\n");
    printf("Signalling OSC...\n");
    printf("Signalling diskstream thread...\n");
    if (pthread_mutex_lock (&diskstreamThreadLock_) == 0) {
            diskstreamThreadContinue_ = false;
            pthread_cond_signal (&diskstreamThreadReady_);
            pthread_mutex_unlock (&diskstreamThreadLock_);
    }
    pthread_join(diskstreamThreadId_,0);
    printf("Signalling Jack...\n");
    stop(); // stop the jack thread
    // TODO stop the diskthread here
    printf("Done\n");
}

void ResoundSession::load_from_xml(const xmlpp::Node* node){
	// throw if we get problems here
	const xmlpp::Element* nodeElement = dynamic_cast<const xmlpp::Element*>(node);
	if(nodeElement)
	{	
		// The construction relies on the xml being ordered correctly
                // you can't refer to objects that have not yet been defined.
		xmlpp::Node::NodeList nodes;
		nodes = nodeElement->get_children();
		xmlpp::Node::NodeList::iterator it;

		for(it = nodes.begin(); it != nodes.end(); ++it){
			const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
			if(child){
				std::string name = child->get_name();
				DynamicObject* p=0;
				if(name=="loudspeaker"){
					p = new Loudspeaker();
				} else if(name=="set"){
					p = new AliasSet();
				} else if(name=="behaviour"){
					p = create_behaviour_from_node(child);
				}
                               
				if(p) p->init_from_xml(child);
			}
		}

	}
	// now loaded so sort out the fast lookup object tables
	build_dsp_object_lookups();

	/// create the disk thread
	pthread_create (&diskstreamThreadId_, NULL, ResoundSession::diskstream_thread, this);

}

Behaviour* ResoundSession::create_behaviour_from_node(const xmlpp::Node* node){
	assert(node);
	const xmlpp::Element* nodeElement = get_element(node);
	ObjectId id = get_attribute_string(nodeElement,"id");
	std::string factoryName = get_attribute_string(nodeElement,"class");
	BehaviourFactoryMap::iterator it = behaviourFactories_.find(factoryName);
	if(it != behaviourFactories_.end()){
		Behaviour* p = (it->second)(); // implicity registers the dynamic object
		return p;
	} else {
		throw Exception("factory not found");
	}
}

void ResoundSession::register_behaviour_factory(std::string name, BehaviourFactory factory){
	assert(factory);
	BehaviourFactoryMap::iterator it = behaviourFactories_.find(name);
	if(it == behaviourFactories_.end()){
		behaviourFactories_[name] = factory;
	} else {
		throw Exception("non-unique name for behaviour factory");
	}
}

void ResoundSession::register_dynamic_object(ObjectId id, DynamicObject* ob){
	assert(ob);
	assert(id != "");
	DynamicObjectMap::iterator it = dynamicObjects_.find(id);
	if(it == dynamicObjects_.end()){
		dynamicObjects_[id] = ob;
		std::cout << "Registered DynamicObject " << id << std::endl;
	} else {
		throw Exception("non-unique name for dynamic object in session");
	}
}

DynamicObject* ResoundSession::get_dynamic_object(ObjectId id){
	assert(id != "");
	// filter out the alias id part so only look for the first part
	size_t pos;
  	pos = id.find('.');    // position of "live" in str
  	ObjectId left = id.substr(0,pos); 
	DynamicObjectMap::iterator it = dynamicObjects_.find(left);
	if(it != dynamicObjects_.end()){
		return it->second;
	} else {
		throw Exception((std::string("Dynamic object with name ") + id + std::string(" not found")).c_str());
	}
}



Loudspeaker* ResoundSession::resolve_loudspeaker(ObjectId id){
	// get the base objects they point to
	
	//check for alias rules
	ObjectId left = id.substr(0,id.find('.'));
        /*
        if(left == "bus"){
            // bus is a keyword so we asume the right hand side is the loudspeaker name
            ObjectId left = id.substr(id.find('.')+1);
        }
         */
        DynamicObject* ob = get_dynamic_object(left);
	// attempt to cast them to get the type we want
	Loudspeaker* loudspeaker = 0;
	loudspeaker = dynamic_cast<Loudspeaker*>(ob);
	if(loudspeaker){
		return loudspeaker;
	}
	throw Exception("Could not resolve to loudspeaker");
}



void ResoundSession::build_dsp_object_lookups(){
	
	// TODO Lock dsp mutex here, this thread should wait for the dsp thread
	loudspeakers_.clear();
	behaviours_.clear();

	DynamicObjectMap::iterator it = dynamicObjects_.begin();
	for(;it != dynamicObjects_.end(); ++it){
		Diskstream* diskstream = dynamic_cast<Diskstream*>( it->second );
		Loudspeaker* loudspeaker = dynamic_cast<Loudspeaker*>( it->second );
		Behaviour* behaviour = dynamic_cast<Behaviour*>( it->second );
		if(diskstream) { diskStreams_.push_back(diskstream); }
		if(loudspeaker) { loudspeakers_.push_back(loudspeaker); }
		if(behaviour) { behaviours_.push_back(behaviour); }

	}
}

int ResoundSession::on_process(jack_nframes_t nframes){

	// if we can, signal to the diskstream thread that more data can probably be loaded
	// TODO put some sort of throttleing on this to allow jack to read a few blocks before bothering to fill buffers
	if (pthread_mutex_trylock (&diskstreamThreadLock_) == 0) {
		pthread_cond_signal (&diskstreamThreadReady_);
		pthread_mutex_unlock (&diskstreamThreadLock_);
	}

	// loudspeakers must be preprocessed to clear buffers
	for(unsigned int n = 0; n < loudspeakers_.size(); ++n){
		loudspeakers_[n]->pre_process(nframes);
	}
	// behaviours sum to buffers
	for(unsigned int n = 0; n < behaviours_.size(); ++n){
		behaviours_[n]->process(nframes);
	}
	// loudspeakers can now be processed out
	for(unsigned int n = 0; n < loudspeakers_.size(); ++n){
		loudspeakers_[n]->post_process(nframes);
	}
	return 0;
}

void ResoundSession::send_osc_feedback(){

	for(unsigned int n = 0; n < loudspeakers_.size(); ++n){
		VUMeter& meter = loudspeakers_[n]->get_vu_meter();
		std::string addr(std::string("/")+loudspeakers_[n]->get_id());
		send_osc_to_all_clients(addr.c_str(),"fff",meter.get_rms(),meter.get_peak(),meter.get_margin(),LO_ARGS_END);
	}
}

/// this should be called from the disk management thread
void ResoundSession::diskstream_process(){
	pthread_mutex_lock (&diskstreamThreadLock_);
        diskstreamThreadContinue_ = true; // this gets switched of on shutdown
	while(diskstreamThreadContinue_){
		//printf("Diskstream load\n");
		for(unsigned int n = 0; n < diskStreams_.size(); ++n){
			diskStreams_[n]->disk_process();
		}
		//now wait for process thread to signal
		pthread_cond_wait (&diskstreamThreadReady_, &diskstreamThreadLock_);

	}
	pthread_mutex_unlock (&diskstreamThreadLock_);
        printf("Diskstream shutdown\n");
}

void* ResoundSession::diskstream_thread (void *arg){
	// cast arg to resound session
	ResoundSession* session = (ResoundSession*) arg;
	session->diskstream_process();
	return 0;
}


void ResoundSession::register_buffer(BufferRef ref){
    assert(ref.id != "");
    assert(ref.id.find('.') != std::string::npos);
    assert(ref.buffer);
    BufferRefMap::iterator it = buffers_.find(ref.id);
    if(it == buffers_.end()){
            buffers_[ref.id] = ref;
            std::cout << "Registered Buffer " << ref.id << std::endl;
    } else {
            throw Exception("non-unique name for buffer in session");
    }
}

BufferRefVector ResoundSession::lookup_buffer(ObjectId id){
    BufferRefVector ret;
    if(id.find('.') != std::string::npos){
        BufferRefMap::iterator it = buffers_.find(id);
        if(it != buffers_.end()){
            ret.push_back(it->second);
        }
    } else {
        for(BufferRefMap::iterator it = buffers_.begin(); it != buffers_.end(); ++it){
            ObjectId s = it->first;
            ObjectId left = s.substr(0,s.find('.'));
            if(id == left){
                ret.push_back(it->second);
            }
            
        }
    }
    return ret;
}

// ------------------------------- Globals and entry point -----------------------------------

CLIOptions g_options;

void parse_command_arguments(int argc, char** argv){
	// making use of boost::program options to deal with command arguments
	namespace po = boost::program_options;

	po::options_description desc("Usage: resoundnv-server");
	desc.add_options()
		("help", "Display this help message.")
		("input", po::value<std::string>(&g_options.inputXML_)->default_value(""), "Input resound xml file, must be set!")
		("port", po::value<std::string>(&g_options.oscPort_)->default_value("8000"), "OSC listening port")
		("test", "Runs some internal testing code")
		//("record", po::value<std::string>(), "Record loudspeakers to wav file <filename>.")
		//("simulate", po::value<int>(), "Loudspeakers are simulated as point sources")

	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);    

	if (vm.count("help")) {
		std::cout << desc << "\n";
		exit(1);
	}

	if (vm.count("test")) {
		test_dsp();
		exit(1);
	}

	if (g_options.inputXML_ == ""){
		std::cout << "Cannot continue, input resound XML file must be specified!\n";
		std::cout << desc << "\n";
		exit(1);
	}

}

ResoundSession* g_session = 0;
bool g_continue = false;

void handle_sigint(int sig){
        signal(SIGINT, handle_sigint);
        g_continue = false;
        printf("Interupted - shutting down\n");
}

int main(int argc, char** argv){

	parse_command_arguments(argc,argv);
	// for now we take the first arg and use it as the filename for xml
	std::cout << "resoundnv server v0.0.1\n";
	std::cout << "Loading config from " << g_options.inputXML_ << std::endl;

	// loading and parsing the xml
//	try
//	{
		xmlpp::DomParser parser;
		parser.set_validate(false);
		parser.set_substitute_entities(); //We just want the text to be resolved/unescaped automatically.
		parser.parse_file(g_options.inputXML_);
		if(parser){
			const xmlpp::Node* pNode = parser.get_document()->get_root_node(); //deleted by DomParser.
			const xmlpp::Element* nodeElement = dynamic_cast<const xmlpp::Element*>(pNode);
			if(nodeElement)
			{	
				std::string name = nodeElement->get_name();
				if(name=="resoundnv"){
					std::cout << "Resoundnv XML node found, building session.\n";
					g_session = new ResoundSession(g_options);
					APP().set_session(g_session);
					SESSION().load_from_xml(nodeElement);
				}
			}
		}
//	}
//	catch(const std::exception& ex){
//		std::cout << "XML Initialisation file parsing exception what()=" << ex.what() << std::endl;
//		std::cout << "Server cannot continue"<< std::endl;
//		std::exit(1);
//	}
        signal(SIGINT, handle_sigint);
        g_continue = true;
	while(g_continue){ // TODO this should really listen for incoming signals, see unix programming book.
		usleep(100000); // around 10 fps
		// use this thread to send some feedback
		//session->update_clients();
                if(g_session) g_session->send_osc_feedback();
	}
        delete g_session; // should invoke destructor
        g_session=0;
        return 0;
}
