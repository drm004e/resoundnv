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




DynamicObject::DynamicObject(const xmlpp::Node* node, ResoundSession* session) :
	session_(session)
{
	assert(session);
	const xmlpp::Element* nodeElement = get_element(node);
	std::string name = nodeElement->get_name();
	id_ = get_attribute_string(nodeElement,"id");
	// attempt to regeister
	session->register_dynamic_object(id_, this);
}

AudioStream::AudioStream(const xmlpp::Node* node, ResoundSession* session) : DynamicObject(node,session)
{
	const xmlpp::Element* nodeElement = get_element(node);
	gain_ = get_optional_attribute_float(nodeElement,"gain",1.0f);	

	jack_nframes_t s = session->get_buffer_size();
	buffer_.allocate(s);
}

Diskstream::Diskstream(const xmlpp::Node* node, ResoundSession* session) : 
		AudioStream(node,session),
		ringBuffer_(0),
		diskBuffer_(0),
		copyBuffer_(0),
		playing_(true)
{
	// diskstream maintains a jack ring buffer and two process functions are called from seperate threads	
	// create a ring buffer	
	// open the file for reading
	// check channels and assert mono
	// read as much as possible into the ring buffer

	const xmlpp::Element* nodeElement = dynamic_cast<const xmlpp::Element*>(node);
	path_ = get_attribute_string(nodeElement,"source");
	ringBuffer_ = jack_ringbuffer_create(DISK_STREAM_RING_BUFFER_SIZE*sizeof(float));
	copyBuffer_ = new float[DISK_STREAM_RING_BUFFER_SIZE];
	memset(copyBuffer_, 0, DISK_STREAM_RING_BUFFER_SIZE * sizeof(float));
	memset(ringBuffer_->buf, 0, ringBuffer_->size); // clear the buffer

	diskBuffer_ = new float[DISK_STREAM_RING_BUFFER_SIZE]; // TODO de-interleaving, really needs an audio pool to be efficient
	memset(diskBuffer_, 0, DISK_STREAM_RING_BUFFER_SIZE * sizeof(float)); // clear the buffer

	file_ = sf_open(path_.c_str(), SFM_READ, &info_);
	if(!file_){
		throw Exception("Disk stream cannot load file, does it exist?");
	}
	if(info_.channels > 1){
		throw Exception("Disk stream cannot load multichannel audio files, use split mono.");
	}

	disk_process();
}

Diskstream::~Diskstream(){
	// free ring buffer, disk buffer and file
	jack_ringbuffer_free(ringBuffer_);
	if(diskBuffer_) delete [] diskBuffer_;
	sf_close(file_);
}

void Diskstream::disk_process(){
	// find out how much space is on the ring buffer available for writing
	// read that much from disk and copy into ring buffer

	if(!playing_){return;}

	size_t bytesToWrite = jack_ringbuffer_write_space(ringBuffer_);
	if(bytesToWrite > 4096){
		bytesToWrite = 1024;
		size_t framesToWrite = bytesToWrite/sizeof(float);
		if (bytesToWrite > 0){
			//printf("bytesToWrite = %i\n",bytesToWrite);
			size_t frames = sf_readf_float(file_, diskBuffer_, framesToWrite);
			// TODO de-interleaving, really needs an audio pool to be efficient
			if( frames < framesToWrite){
				printf("Silence\n");
				// fill with silence, happens at end of file and thereafter
				for(size_t n = frames; n < framesToWrite; ++n){
					diskBuffer_[n] = 0.0f;
				}
			}
			//avg_signal_in_buffer(diskBuffer_,framesToWrite); // audio tested and arrives here
			size_t bytesWritten = jack_ringbuffer_write(ringBuffer_, (char*)diskBuffer_, bytesToWrite);
			//printf("DiskBuffer read %i frames from disk and wrote %i bytes\n",frames,bytesWritten);
		}
	}
	
	// this function is only worth calling if a jack process has happened so we use cond_wait for this in master thread
	// however this function is also called on init so we cant wait here.
}

void Diskstream::process(jack_nframes_t nframes){
	// find out how much space is available for reading on the buffer
	// if we have enough for the whole block we are ok
	// otherwise we have to call it a buffer underrun
	// write as much as we can then fill with zeros
	// The problem here is that this disk stream will now be out of playback sync by a number of samples
	// we need to skip those on the next buffer, is there any point attempting to get back in sync? we have already glitched
	//float tbuffer[4096];

	if(!playing_){return;}

	size_t bytesToRead = nframes * sizeof(float);
	//printf("bytesToRead = %i\n",bytesToRead);
	size_t rSpace = jack_ringbuffer_read_space (ringBuffer_);
	if(rSpace >= bytesToRead){
		size_t bytesRead = jack_ringbuffer_read (ringBuffer_, (char*)copyBuffer_, bytesToRead);
		ab_copy_with_gain(copyBuffer_, get_buffer()->get_buffer(),nframes, get_gain());
		//size_t bytesRead = jack_ringbuffer_read (ringBuffer_, (char*)tbuffer, bytesToRead);
		//TODO gain should be applied here
		//printf("Buffer read %i bytes, from %i available\n",bytesRead, rSpace);
	} else {
		// buffer underrun
		printf("Buffer underrun!\n");
	}
	//avg_signal_in_buffer(get_buffer()->get_buffer(),nframes);
	//avg_signal_in_buffer(tbuffer,nframes);
	get_vu_meter().analyse_buffer(get_buffer()->get_buffer(),nframes);

}

void Diskstream::seek(size_t pos){
	sf_seek(file_, pos, SEEK_SET);
}

void Diskstream::play(){
	playing_ = true;
}
void Diskstream::stop(){
	playing_ = false;
	jack_ringbuffer_reset(ringBuffer_);
	
}

Livestream::Livestream(const xmlpp::Node* node, ResoundSession* session) : AudioStream(node,session)
{	
	// this node will need to establish a jack stream via the jack system, it should kill the port when done
	const xmlpp::Element* nodeElement = get_element(node);
	connectionName_ = get_attribute_string(nodeElement,"port");
	port_ = new JackPort(get_id(), JackPortIsInput ,&get_session());
	port_->connect(connectionName_);

	std::cout << "Livestream " << get_id() << std::endl;
}
void Livestream::process(jack_nframes_t nframes){
	// copy from jack buffer applying gain
	ab_copy_with_gain(port_->get_audio_buffer(nframes), get_buffer()->get_buffer(),nframes, get_gain());

	//avg_signal_in_buffer(get_buffer()->get_buffer(),nframes); // sound tested here

	//TODO optional vumetering
	get_vu_meter().analyse_buffer(get_buffer()->get_buffer(),nframes);
};

Loudspeaker::Loudspeaker(const xmlpp::Node* node, ResoundSession* session) : DynamicObject(node,session)
{
	// this node will need to establish a jack stream via the jack system, it should kill the port when done
	const xmlpp::Element* nodeElement = get_element(node);
	connectionName_ = get_attribute_string(nodeElement,"port");
	port_ = new JackPort(get_id(), JackPortIsOutput ,&get_session());
	port_->connect(connectionName_);

	type_ = get_optional_attribute_string(nodeElement,"type");
	pos_.x = get_optional_attribute_float(nodeElement,"x");
	pos_.y = get_optional_attribute_float(nodeElement,"y");
	pos_.z = get_optional_attribute_float(nodeElement,"z");
	az_ = get_optional_attribute_float(nodeElement,"az");
	el_ = get_optional_attribute_float(nodeElement,"el");
	
	gain_ = get_optional_attribute_float(nodeElement,"gain",1.0);

	std::cout << "Loudspeaker " << get_id() << " type=" << type_ << std::endl;

	jack_nframes_t s = session->get_buffer_size();
	buffer_.allocate(s);
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

Alias::Alias(const xmlpp::Node* node){
	const xmlpp::Element* nodeElement = get_element(node);
	id_= get_attribute_string(nodeElement,"id");
	ref_= get_attribute_string(nodeElement,"ref");
	gain_ = get_optional_attribute_float(nodeElement,"gain",1.0f);
}

AliasSet::AliasSet(const xmlpp::Node* node, ResoundSession* session) : DynamicObject(node,session)
{
	// look for aliases
	const xmlpp::Element* nodeElement = get_element(node);
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="alias"){
				Alias* alias = new Alias(child);
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
}

Alias* AliasSet::get_alias(ObjectId id){
	AliasMap::iterator it = aliases_.find(id);
	if(it != aliases_.end()){
		return it->second;
	} else {
		throw Exception((std::string("Alias not found : ") + id).c_str());
	}
}

CASS::CASS(const xmlpp::Node* node, ResoundSession* session) : AliasSet(node,session)
{
	std::cout << "CASS : " << get_id() << std::endl;
}

CLS::CLS(const xmlpp::Node* node, ResoundSession* session) : AliasSet(node,session)
{
	std::cout << "CLS : " << get_id() << std::endl;
}


ResoundSession::ResoundSession(CLIOptions options) : 
		Resound::OSCManager(options.oscPort_.c_str()),
		options_(options) {

	// diskstream threads
	pthread_mutex_init (&diskstreamThreadLock_, NULL);
	pthread_cond_init(&diskstreamThreadReady_, NULL);

	// registering some factories
	register_behaviour_factory("att", AttBehaviour::factory);
	register_behaviour_factory("mpc", MultipointCrossfadeBehaviour::factory);
	register_behaviour_factory("chase", ChaseBehaviour::factory);
	register_behaviour_factory("amppan", AmpPanBehaviour::factory);

	register_behaviour_factory("minimal", MinimalRouteSetBehaviour::factory);
	register_behaviour_factory("iobehaviour", MinimalIOBehaviour::factory); // for now

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
		// The construction must occur in stages because
		// certain elements need to be in place for others to refer to them.	
		xmlpp::Node::NodeList nodes;
		nodes = nodeElement->get_children();
		xmlpp::Node::NodeList::iterator it;

		// stage 1 - build streams and loudspeakers
		for(it = nodes.begin(); it != nodes.end(); ++it){
			const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
			if(child){
				std::string name = child->get_name();
				if(name=="diskstream"){
					new Diskstream(child,this);
				} else if(name=="livestream"){
					new Livestream(child,this);
				} else if(name=="loudspeaker"){
					new Loudspeaker(child,this);
				} else {
				}
			}
		}
		// stage 2 - build cass and cls objects
		for(it = nodes.begin(); it != nodes.end(); ++it){
			const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
			if(child){
				std::string name = child->get_name();
				if(name=="cass"){

					new CASS(child,this);
				} else if(name=="cls"){
					new CLS(child,this);
				}else {
				}
			}
		}
		// stage 3 - build behaviours
		for(it = nodes.begin(); it != nodes.end(); ++it){
			const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
			if(child){
				std::string name = child->get_name();
				if(name=="behaviour"){
					create_behaviour_from_node(child);
				} else {
				}
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
		Behaviour* p = (it->second)(node,this); // implicity registers the dynamic object
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

AudioStream* ResoundSession::resolve_audiostream(ObjectId id){
	// get the base objects they point to
	DynamicObject* ob = get_dynamic_object(id);
	//check for alias rules
	bool isAlias = id.find('.') != std::string::npos;
	// attempt to cast them to get the type we want
	AudioStream* audioStream = 0;
	CASS* cass = 0;
	
	audioStream = dynamic_cast<AudioStream*>(ob);
	if(audioStream){
		return audioStream;
	} else {
		cass = dynamic_cast<CASS*>(ob);
		if(cass && isAlias){
			ObjectId aliasId = id.substr(id.find('.')+1);
			// resolve the alias to a stream and set
			audioStream = dynamic_cast<AudioStream*>(
							get_dynamic_object(cass->get_alias(aliasId)->get_ref())
						);
			if(audioStream) {
				return audioStream;
			}
		}
	}
	throw Exception("Could not resolve to audio stream");
}

Loudspeaker* ResoundSession::resolve_loudspeaker(ObjectId id){
	// get the base objects they point to
	DynamicObject* ob = get_dynamic_object(id);
	//check for alias rules
	bool isAlias = id.find('.') != std::string::npos;
	// attempt to cast them to get the type we want
	Loudspeaker* loudspeaker = 0;
	CLS* cls = 0;
	
	loudspeaker = dynamic_cast<Loudspeaker*>(ob);
	if(loudspeaker){
		return loudspeaker;
	} else {
		cls = dynamic_cast<CLS*>(ob);
		if(cls && isAlias){
			ObjectId aliasId = id.substr(id.find('.')+1);
			// resolve the alias to a stream and set
			loudspeaker = dynamic_cast<Loudspeaker*>(
							get_dynamic_object(cls->get_alias(aliasId)->get_ref())
						);
			if(loudspeaker) {
				return loudspeaker;
			}
		}
	}
	throw Exception("Could not resolve to loudspeaker");
}



void ResoundSession::build_dsp_object_lookups(){
	
	// TODO Lock dsp mutex here, this thread should wait for the dsp thread

	audioStreams_.clear();
	loudspeakers_.clear();
	behaviours_.clear();

	DynamicObjectMap::iterator it = dynamicObjects_.begin();
	for(;it != dynamicObjects_.end(); ++it){
		AudioStream* stream = dynamic_cast<AudioStream*>( it->second );
		Diskstream* diskstream = dynamic_cast<Diskstream*>( it->second );
		Loudspeaker* loudspeaker = dynamic_cast<Loudspeaker*>( it->second );
		Behaviour* behaviour = dynamic_cast<Behaviour*>( it->second );
		if(stream) { audioStreams_.push_back(stream); }
		if(diskstream) { diskStreams_.push_back(diskstream); }
		if(loudspeaker) { loudspeakers_.push_back(loudspeaker); }
		if(behaviour) { behaviours_.push_back(behaviour); }

	}
}

int ResoundSession::on_process(jack_nframes_t nframes){

	// TODO need mutex arangment here for object creation and destruction
	for(unsigned int n = 0; n < audioStreams_.size(); ++n){
		audioStreams_[n]->process(nframes);
	}
	
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
	for(unsigned int n = 0; n < audioStreams_.size(); ++n){
		VUMeter& meter = audioStreams_[n]->get_vu_meter();
		std::string addr(std::string("/")+audioStreams_[n]->get_id());
		send_osc_to_all_clients(addr.c_str(),"fff",meter.get_rms(),meter.get_peak(),meter.get_margin());
	}

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
					g_session->load_from_xml(nodeElement);
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
