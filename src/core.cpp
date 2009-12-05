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

AudioBuffer::AudioBuffer() : buffer_(0) {
}
AudioBuffer::~AudioBuffer(){
	destroy();

}
void AudioBuffer::allocate( size_t size ){
	destroy();
	buffer_ = new float[size];
	size_ = size;
	
}
void AudioBuffer::clear(){
	std::memset(buffer_, 0, sizeof(float) * size_);
}
void AudioBuffer::destroy(){
	if( buffer_ ) delete [] buffer_;
	buffer_ = 0;
	size_ = 0;
}

void ab_copy(const float* src, float* dest, size_t N ){
	std::memcpy(dest, src, sizeof(float) * N);
}
void ab_copy_with_gain(const float* src, float* dest, size_t N, float gain){
	for(size_t n=0; n < N; ++n){
		dest[n] = src[n] * gain;
	}
}
void ab_sum_with_gain(const float* src, float* dest, size_t N, float gain){
	for(size_t n=0; n < N; ++n){
		dest[n] += src[n] * gain;
	}
}

void avg_signal_in_buffer(const float* src, size_t N){
	float peak = 0.0f;
	float sum = 0.0f;
	for(size_t n=0; n < N; ++n){
		float v = std::abs(src[n]);
		sum += v;
		if(peak < v) peak = v;
	}
	printf("RMS %f  Peak %f\n", sum/float(N), peak);
}


const xmlpp::Element* get_element(const xmlpp::Node* node){
	const xmlpp::Element* nodeElement = dynamic_cast<const xmlpp::Element*>(node);
	if(nodeElement){
		return nodeElement;
	} else {
		throw Exception("Not possible to cast to an element node");
	}
}
std::string get_attribute_string(const xmlpp::Element* node, const std::string& name){
	const xmlpp::Attribute* attribute = node->get_attribute(name);
	if(attribute){
		return attribute->get_value();
	} else {
		std::string msg("Couldn't find an attribute by name : ");
		throw Exception((msg+name).c_str()); 
	}
}
std::string get_optional_attribute_string(const xmlpp::Element* node, const std::string& name, std::string def=std::string()){
	const xmlpp::Attribute* attribute = node->get_attribute(name);
	if(attribute){
		return attribute->get_value();
	} else {
		return def;
	}
}
float get_optional_attribute_float(const xmlpp::Element* node, const std::string& name, float def=0.0f){
	const xmlpp::Attribute* attribute = node->get_attribute(name);
	if(attribute){
		return atof(attribute->get_value().c_str());
	} else {
		return def;
	}
}

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

Diskstream::Diskstream(const xmlpp::Node* node, ResoundSession* session) : AudioStream(node,session)
{
	// this node will need to establish a diskstream from a filename, probably via some audio pool
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
};

Loudspeaker::Loudspeaker(const xmlpp::Node* node, ResoundSession* session) : DynamicObject(node,session)
{
	// this node will need to establish a jack stream via the jack system, it should kill the port when done
	const xmlpp::Element* nodeElement = get_element(node);
	connectionName_ = get_attribute_string(nodeElement,"port");
	port_ = new JackPort(get_id(), JackPortIsOutput ,&get_session());
	port_->connect(connectionName_);

	type_ = get_optional_attribute_string(nodeElement,"type");
	x_ = get_optional_attribute_float(nodeElement,"x");
	y_ = get_optional_attribute_float(nodeElement,"y");
	z_ = get_optional_attribute_float(nodeElement,"z");
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

BRouteSet::BRouteSet(const xmlpp::Node* node, ResoundSession* session){
	// a routeset works a little bit like a collective
	// it contains any number of routes grouped together
	std::cout << "Found route set" << std::endl;

	const xmlpp::Element* nodeElement = get_element(node);
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="route"){
				std::cout << "Found a route" << std::endl;
				// determine one of four input to output situations:
				// 1) single to single, simple, create a direct path
				// 2) set to single, every alias of the set is connected to the single
				// 3) single to set, the single is connected to every alias of the set
				// 4) set to set, connect matching alias names like for like
				// 5).... further rules may be added possibly with regex matching
				
				// get the from and to attributes
				
				// for each one
				// check for alias rules (.)
				// find out what the first part of the id refers to
				// check to see if its a valid address
				// if the address is invalid for either 'from' or 'to' then discard the whole route
				// check for 'from' pointing to stream, cass or cass alias
				// check for 'to' pointing to loudspeaker, cls or cls alias
				// left and right group size is available at this point
				// if cass alias then resolve the alias against the cass
				// if cls alias then resolve the alias against the cls

				// with the group sizes known then we can go through matching aliases

				// get the from and to attributes
				ObjectId from = get_attribute_string(child,"from");
				ObjectId to = get_attribute_string(child,"to");
				float gain = get_optional_attribute_float(child,"gain");

				//check for alias rules
				bool fromIsAlias = from.find('.') != std::string::npos;
				bool toIsAlias = from.find('.') != std::string::npos;

				// get the objects they point to
				DynamicObject* fromObject = session->get_dynamic_object(from);
				DynamicObject* toObject = session->get_dynamic_object(to);

				// attempt to cast them to get the type we want
				AudioStream* audioStream = 0;
				CASS* cass = 0;
				Loudspeaker* loudspeaker = 0;
				CLS* cls = 0;
				
				audioStream = dynamic_cast<AudioStream*>(fromObject);
				if(audioStream){
					std::cout << "From AudioStream ";
				} else {
					cass = dynamic_cast<CASS*>(fromObject);
					if(cass){
						if(fromIsAlias){
							std::cout << "From CASS alias ";
							ObjectId aliasId = from.substr(from.find('.')+1);
							// resolve the alias to a stream and set
							audioStream = dynamic_cast<AudioStream*>(
											session->get_dynamic_object(cass->get_alias(aliasId)->get_ref())
										);
							cass = 0;
						} else {
							std::cout << "From CASS ";
						}
					} else {
						throw Exception("From not a stream or cass object");
					}
				}

				// attempt to cast them to get the type we want
				loudspeaker = dynamic_cast<Loudspeaker*>(toObject);
				if(loudspeaker){
					std::cout << "to Loudspeaker" << std::endl;
				} else {
					cls = dynamic_cast<CLS*>(toObject);
					if(cls){
						if(toIsAlias){
							std::cout << "to CLS alias" << std::endl;
							ObjectId aliasId = from.substr(from.find('.')+1);
							// resolve the alias to a stream and set
							loudspeaker = dynamic_cast<Loudspeaker*>(
											session->get_dynamic_object(cls->get_alias(aliasId)->get_ref())
										);
							cls = 0;
						} else {
							std::cout << "to CLS" << std::endl;
						}
					} else {
						throw Exception("From not a loudspeaker or cls object");
					}
				}

				// at this point we have all objects concerned, aliases have been resolved
				// deal with each of the situations
				// multiple calls get made to the following
				// routes_.push_back(Route(from, to));

				if( audioStream && loudspeaker ){
					std::cout << "Building route: " << audioStream->get_id() << " -> "<< loudspeaker->get_id() << std::endl;
					routes_.push_back(BRoute(audioStream->get_buffer(),loudspeaker->get_buffer(), gain ));

				} else if ( cass && loudspeaker){
					const AliasMap& cassAliases = cass->get_aliases();
					for( AliasMap::const_iterator it = cassAliases.begin();
							it != cassAliases.end();
								++it){
						AudioStream* s = dynamic_cast<AudioStream*>( session->get_dynamic_object(it->second->get_ref()) );
						std::cout << "Building route: " << s->get_id() << " -> "<< loudspeaker->get_id() << std::endl;
						routes_.push_back(BRoute(s->get_buffer(),loudspeaker->get_buffer(), gain ));
					}
				} else if ( audioStream && cls){
					const AliasMap& clsAliases = cls->get_aliases();
					for( AliasMap::const_iterator it = clsAliases.begin();
							it != clsAliases.end();
								++it){
						Loudspeaker* l = dynamic_cast<Loudspeaker*>( session->get_dynamic_object(it->second->get_ref()) );
						std::cout << "Building route: " << audioStream->get_id() << " -> "<< l->get_id() << std::endl;
						routes_.push_back(BRoute(audioStream->get_buffer(),l->get_buffer(), gain ));
					}					
				} else if ( cass && cls){
					const AliasMap& cassAliases = cass->get_aliases();
					for( AliasMap::const_iterator it = cassAliases.begin();
							it != cassAliases.end();
								++it){
						AudioStream* s = dynamic_cast<AudioStream*>( session->get_dynamic_object(it->second->get_ref()) );
						// for each stream we need to check for a matching alias by requestic one of the same name
						ObjectId streamAliasId = it->second->get_id();
						Alias* loudspeakerAlias = cls->get_alias(streamAliasId);
						Loudspeaker* l = dynamic_cast<Loudspeaker*>( session->get_dynamic_object(loudspeakerAlias->get_ref()) );
						std::cout << "Building route: " << from << "." << streamAliasId << " " << s->get_id() 
														<< " -> "
														<< to << "." << loudspeakerAlias->get_id() << " " << l->get_id() << std::endl;
						routes_.push_back(BRoute(s->get_buffer(),l->get_buffer(), gain ));
					}		
					
				}
			}
		}	
	}
}
BParam::BParam(const xmlpp::Node* node, ResoundSession* session){
	const xmlpp::Element* nodeElement = get_element(node);
	id_ = get_attribute_string(nodeElement,"id");
	addr_ = get_attribute_string(nodeElement,"address");
	std::cout << "Found parameter "<<id_<< " osc address " << addr_ << std::endl;
	// at this point we should register the parameter address with osc
	session->add_method(addr_,"f", BParam::lo_cb_params, this);	
}

int BParam::lo_cb_params(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data){
	BParam* param = static_cast<BParam*>(user_data);
	param->value_ = argv[0]->f; // TODO validation here required
	std::cout << "OSC BParam "<< path<< " " <<param->value_<< std::endl;
    return 1;
}

Behaviour::Behaviour(const xmlpp::Node* node, ResoundSession* session) : DynamicObject(node,session)
{
	// setup various osc parameters
	const xmlpp::Element* nodeElement = get_element(node);
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="param"){
				BParam* param = new BParam(child,session);
				ObjectId id = param->get_id();
				BParamMap::iterator it = params_.find(id);
				if(it == params_.end()){
					params_[id] = param;
		
				} else {
					throw Exception("non-unique parameter name");
				}	
			} 
		}	
	}
}


RouteSetBehaviour::RouteSetBehaviour(const xmlpp::Node* node, ResoundSession* session) : Behaviour(node,session) {
	// This class of behaviour uses the routset interpretation for CASS and CLS
	const xmlpp::Element* nodeElement = get_element(node);
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="routeset"){
				BRouteSet* routeSet = new BRouteSet(child,session);
				routeSets_.push_back(routeSet);
			}
		}	
	}
}

IOBehaviour::IOBehaviour(const xmlpp::Node* node, ResoundSession* session) : Behaviour(node,session) {
	// TODO this is incomplete
	// This class of behaviour uses the i/o interpretation for CASS and CLS
	const xmlpp::Element* nodeElement = get_element(node);
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="input"){
				//BRouteSet* routeSet = new BRouteSet(child);
				//routeSets_.push_back(routeSet);
			} else if(name=="output"){
			}
		}	
	}
}

void AttBehaviour::process(jack_nframes_t nframes){
	// ok we would get all the routes for the first routeset
	// then we do buffer copy for each one applying our current gain setting
	float level = get_parameter_value("level");
	// only interested in the first routeset
	const BRouteSetArray& routeSets = get_route_sets();
	if(routeSets.size() > 0){
		const BRouteArray& routes = routeSets[0]->get_routes();
		for(unsigned int n = 0; n < routes.size(); ++n){
			AudioBuffer* from = routes[n].get_from();
			
			//printf("Route %i from",n); avg_signal_in_buffer(from->get_buffer(),nframes); // signal tested to here
			AudioBuffer* to = routes[n].get_to();
			float gain = routes[n].get_gain();
			ab_sum_with_gain(from->get_buffer(), to->get_buffer(), nframes, gain * level);
			//std::cout << "AttBehaviour::process - single route" << std::endl;
			 
			//printf("Route %i to",n); avg_signal_in_buffer(to->get_buffer(),nframes); // signal tested to here
		}
	}
	//std::cout << "AttBehaviour::process" << std::endl;
}

ResoundSession::ResoundSession() : Resound::OSCManager("8000") {
	// registering some factories
	register_behaviour_factory("minimal", MinimalRouteSetBehaviour::factory);
	register_behaviour_factory("att", AttBehaviour::factory); // for now
	register_behaviour_factory("iobehaviour", MinimalIOBehaviour::factory); // for now
	init("resoundnv-session");
	start();
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

void ResoundSession::build_dsp_object_lookups(){
	
	// TODO Lock dsp mutex here, this thread should wait for the dsp thread

	audioStreams_.clear();
	loudspeakers_.clear();
	behaviours_.clear();

	DynamicObjectMap::iterator it = dynamicObjects_.begin();
	for(;it != dynamicObjects_.end(); ++it){
		AudioStream* stream = dynamic_cast<AudioStream*>( it->second );
		Loudspeaker* loudspeaker = dynamic_cast<Loudspeaker*>( it->second );
		Behaviour* behaviour = dynamic_cast<Behaviour*>( it->second );
		if(stream) { audioStreams_.push_back(stream); }
		if(loudspeaker) { loudspeakers_.push_back(loudspeaker); }
		if(behaviour) { behaviours_.push_back(behaviour); }

	}
}

int ResoundSession::on_process(jack_nframes_t nframes){
	// TODO need mutex arangment here for object creation and destruction
	for(unsigned int n = 0; n < audioStreams_.size(); ++n){
		audioStreams_[n]->process(nframes);
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



void load_from_xml(const std::string path){
	// loading and parsing the xml
//	try
//	{
		xmlpp::DomParser parser;
		parser.set_validate(false);
		parser.set_substitute_entities(); //We just want the text to be resolved/unescaped automatically.
		parser.parse_file(path);
		if(parser){
			const xmlpp::Node* pNode = parser.get_document()->get_root_node(); //deleted by DomParser.
			const xmlpp::Element* nodeElement = dynamic_cast<const xmlpp::Element*>(pNode);
			if(nodeElement)
			{	
				std::string name = nodeElement->get_name();
				if(name=="resoundnv"){
					std::cout << "Resoundnv XML node found, building session.\n";
					ResoundSession* session = new ResoundSession();
					session->load_from_xml(nodeElement);
				}
			}
		}
//	}
//	catch(const std::exception& ex){
//		std::cout << "XML Initialisation file parsing exception what()=" << ex.what() << std::endl;
//		std::cout << "Server cannot continue"<< std::endl;
//		std::exit(1);
//	}
}



int main(int argc, char** argv){

	// for now we take the first arg and use it as the filename for xml
	std::cout << "resoundnv server v0.0.1\n";
	if(argc < 2){ 
		std::cout << "You must specify a config file!\n\tUsage: resoundnv-server file.xml"<<std::endl; 
		exit(1);
	}
	std::cout << "Loading config from " << argv[1] << std::endl;
	load_from_xml(argv[1]);
	while(1){
		usleep(10000);
	}
}
