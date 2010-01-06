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

#include "resoundnv/behaviour.hpp"
#include "resoundnv/core.hpp"

BRouteSet::BRouteSet(const xmlpp::Node* node){
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
				// 3) single to set,oldGains_[o] the single is connected to every alias of the set
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
				DynamicObject* fromObject = SESSION().get_dynamic_object(from);
				DynamicObject* toObject = SESSION().get_dynamic_object(to);

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
											SESSION().get_dynamic_object(cass->get_alias(aliasId)->get_ref())
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
											SESSION().get_dynamic_object(cls->get_alias(aliasId)->get_ref())
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
						AudioStream* s = dynamic_cast<AudioStream*>( SESSION().get_dynamic_object(it->second->get_ref()) );
						std::cout << "Building route: " << s->get_id() << " -> "<< loudspeaker->get_id() << std::endl;
						routes_.push_back(BRoute(s->get_buffer(),loudspeaker->get_buffer(), gain ));
					}
				} else if ( audioStream && cls){
					const AliasMap& clsAliases = cls->get_aliases();
					for( AliasMap::const_iterator it = clsAliases.begin();
							it != clsAliases.end();
								++it){
						Loudspeaker* l = dynamic_cast<Loudspeaker*>( SESSION().get_dynamic_object(it->second->get_ref()) );
						std::cout << "Building route: " << audioStream->get_id() << " -> "<< l->get_id() << std::endl;
						routes_.push_back(BRoute(audioStream->get_buffer(),l->get_buffer(), gain ));
					}
				} else if ( cass && cls){
					const AliasMap& cassAliases = cass->get_aliases();
					for( AliasMap::const_iterator it = cassAliases.begin();
							it != cassAliases.end();
								++it){
						AudioStream* s = dynamic_cast<AudioStream*>( SESSION().get_dynamic_object(it->second->get_ref()) );
						// for each stream we need to check for a matching alias by requestic one of the same name
						ObjectId streamAliasId = it->second->get_id();
						Alias* loudspeakerAlias = cls->get_alias(streamAliasId);
						Loudspeaker* l = dynamic_cast<Loudspeaker*>( SESSION().get_dynamic_object(loudspeakerAlias->get_ref()) );
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
BParam::BParam(const xmlpp::Node* node){
	const xmlpp::Element* nodeElement = get_element(node);
	id_ = get_attribute_string(nodeElement,"id");
	addr_ = get_attribute_string(nodeElement,"address");
	value_ = get_optional_attribute_float(nodeElement,"value");
	std::cout << "Found parameter "<<id_<< " osc address " << addr_ << std::endl;
	// at this point we should register the parameter address with osc
	SESSION().add_method(addr_,"f", BParam::lo_cb_params, this);
}

int BParam::lo_cb_params(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data){
	BParam* param = static_cast<BParam*>(user_data);
	param->value_ = argv[0]->f; // TODO validation here required
	//std::cout << "OSC BParam "<< path<< " " <<param->value_<< std::endl; // debug print
    return 1;
}

Behaviour::Behaviour() 
{}
Behaviour::~Behaviour(){
    for(int n = 0; n < buffers_.size(); ++n){
        delete buffers_[n];
    }
}

void Behaviour::init_from_xml(const xmlpp::Element* nodeElement){
	// setup various osc parameters
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="param"){
				BParam* param = new BParam(child);
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
	DynamicObject::init_from_xml(nodeElement);
}

void Behaviour::create_buffer(ObjectId subId){
    AudioBuffer* b = new AudioBuffer();
    jack_nframes_t s = SESSION().get_buffer_size();
    b->allocate(s);
    BufferRef ref;
    std::stringstream str;
    if(subId != ""){
        str << get_id() << "." << subId;
    } else {
        str << get_id() << "." << buffers.size();
    }
    ref.id =  str.str();
    ref.isAlias = false;
    ref.isBus = false;
    ref.creator = get_id();
    ref.buffer = b;
    SESSION().register_buffer(ref);
    buffers_.push_back(b);
    
}


RouteSetBehaviour::RouteSetBehaviour(){}
void RouteSetBehaviour::init_from_xml(const xmlpp::Element* nodeElement){
	// This class of behaviour uses the routset interpretation for CASS and CLS
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="routeset"){
				BRouteSet* routeSet = new BRouteSet(child);
				routeSets_.push_back(routeSet);
			}
		}
	}
	Behaviour::init_from_xml(nodeElement);
}

IOBehaviour::IOBehaviour(){}
void IOBehaviour::init_from_xml(const xmlpp::Element* nodeElement){
	// TODO Although basic inputs and outputs are created this does not consider cass or cls as groups
	// This class of behaviour uses the i/o interpretation for CASS and CLS
	xmlpp::Node::NodeList nodes;
	nodes = nodeElement->get_children();
	xmlpp::Node::NodeList::iterator it;
	for(it = nodes.begin(); it != nodes.end(); ++it){
		const xmlpp::Element* child = dynamic_cast<const xmlpp::Element*>(*it);
		if(child){
			std::string name = child->get_name();
			if(name=="input"){
				ObjectId id = get_attribute_string(child,"ref");
				AudioStream* stream = SESSION().resolve_audiostream(id);
				inputs_.push_back(stream);
			} else if(name=="output"){
				ObjectId id = get_attribute_string(child,"ref");
				Loudspeaker* loudspeaker = SESSION().resolve_loudspeaker(id);
				outputs_.push_back(loudspeaker);
			}
		}
	}
	Behaviour::init_from_xml(nodeElement);
}

void AttBehaviour::process(jack_nframes_t nframes){
	// ok we would get all the routes for the first routeset
	// then we do buffer copy for each one applying our current gain setting
	float level = get_parameter_value("level");
	// only interested in the first routeset
	BRouteSetArray& routeSets = get_route_sets();
	if(routeSets.size() > 0){
		BRouteArray& routes = routeSets[0]->get_routes();
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

MultipointCrossfadeBehaviour::MultipointCrossfadeBehaviour(){}

void MultipointCrossfadeBehaviour::init_from_xml(const xmlpp::Element* nodeElement){

	std::cout << "Created MultipointCrossfade routeset behaviour object!" << std::endl;
	// right much work for this algorithm is based on the concept of scaling, offseting and clipping the range 0 - 1
	// see pd patch phasor-chase-algorithm.pd

	// OSC BParams for fader "position" 0 - 1, gain factor "gain" and "slope", slope controls the mapping function

	// Algorithm uses scales and offsets the position range such that we get a set of 0-1 indexs for each of the routesets
	// the indexs are used to table lookup into a hanning window function which is then applied modified by apropriate factors.



	hannFunction = LookupTable::create_hann(HANN_TABLE_SIZE);

	RouteSetBehaviour::init_from_xml(nodeElement);	

	position_ = get_parameter_value("position");
	gain_= get_parameter_value("gain");
	slope_= get_parameter_value("slope");
	
}
void MultipointCrossfadeBehaviour::process(jack_nframes_t nframes){



	BRouteSetArray& routeSets = get_route_sets();
	int numRoutes = routeSets.size();
	float N = (float)numRoutes;
	slope_= clip(get_parameter_value("slope"),1,1000); // TODO this would be better with a clip_lower_bound function

	position_ = clip(get_parameter_value("position"),0,1) * slope_;
	gain_= get_parameter_value("gain");

	float f = slope_;
	// offset factor (1-1/f)/(N-1)*f
	float offsetFactor = (1.0f - 1.0f/f)/(N-1.0f)*f;

	if(numRoutes > 0){
		for(int setNum = 0; setNum < numRoutes; ++setNum){
			float i = zero_outside_bounds( position_ - offsetFactor * (float)setNum, 0.0f, 1.0f);
			float routeSetGain = hannFunction->lookup_linear(i * (float)HANN_TABLE_SIZE ) * gain_;

			BRouteArray& routes = routeSets[setNum]->get_routes();

			// dsp for each route
			for(unsigned int n = 0; n < routes.size(); ++n){
				// old gain data is stored in the userData
				float* oldGain = (float*)routes[n].get_user_data();
				if(!oldGain) {
					oldGain	= new float(0.0f);
					routes[n].set_user_data(oldGain);
				}
				AudioBuffer* from = routes[n].get_from();
				AudioBuffer* to = routes[n].get_to();
				float gain = routes[n].get_gain() * routeSetGain;

				ab_sum_with_gain_linear_interp(from->get_buffer(), to->get_buffer(), nframes, gain, *oldGain, 128);
				*oldGain = gain;
			}
		}
	}

}
ChaseBehaviour::ChaseBehaviour() : phasor(44100.0f/128.0f,1){}

void ChaseBehaviour::init_from_xml(const xmlpp::Element* nodeElement){
	std::cout << "Created ChaseBehaviour routeset behaviour object!" << std::endl;
	// this is based on the multipoint crossfader but uses a phasor to control position
	hannFunction = LookupTable::create_hann(HANN_TABLE_SIZE);

	RouteSetBehaviour::init_from_xml(nodeElement);

	freq_ = get_parameter_value("freq");
	phase_ = get_parameter_value("phase");
	gain_= get_parameter_value("gain");
	slope_= get_parameter_value("slope");

	phasor.set_phase(phase_);
}
void ChaseBehaviour::process(jack_nframes_t nframes){

	BRouteSetArray& routeSets = get_route_sets();
	int numRoutes = routeSets.size();
	float N = (float)numRoutes;
	slope_= clip(get_parameter_value("slope"),1,1000); // TODO this would be better with a clip_lower_bound function

	// TODO this is ineficient and should really use a messaging callback system
	freq_ = clip(get_parameter_value("freq"),-50,50);
	phasor.set_freq(freq_);

	// the chase gets its position from the phasor
	float phase = clip(phasor.get_phase(),0,1);
	phasor.tick();

	gain_= get_parameter_value("gain");

	float f = slope_;

	if(numRoutes > 0){
		float offsetFactor = 1.0f / N;
		for(int setNum = 0; setNum < numRoutes; ++setNum){	
			float p = wrap(phase - setNum*offsetFactor) * TWOPI;
			float routeSetGain = pow(cos(p) * 0.5f + 0.5f,f) * gain_;

			BRouteArray& routes = routeSets[setNum]->get_routes();

			// dsp for each route
			for(unsigned int n = 0; n < routes.size(); ++n){
				// old gain data is stored in the userData
				float* oldGain = (float*)routes[n].get_user_data();
				if(!oldGain) {
					oldGain	= new float(0.0f);
					routes[n].set_user_data(oldGain);
				}
				AudioBuffer* from = routes[n].get_from();
				AudioBuffer* to = routes[n].get_to();
				float gain = routes[n].get_gain() * routeSetGain;

				ab_sum_with_gain_linear_interp(from->get_buffer(), to->get_buffer(), nframes, gain, *oldGain, 128);
				*oldGain = gain;
			}
		}
	}

}

AmpPanBehaviour::AmpPanBehaviour(){}

void AmpPanBehaviour::init_from_xml(const xmlpp::Element* nodeElement){

	IOBehaviour::init_from_xml(nodeElement);

	std::cout << "Created AmpPan io based behaviour object!" << std::endl;
	pos_.x = get_parameter_value("x");
	pos_.y = get_parameter_value("y");
	pos_.z = get_parameter_value("z");
	gain_ = get_parameter_value("gain");

	// identify the number off outputs and get each ones gain
	// setup storage for previous gain suitable for interpolation
	LoudspeakerArray& outputs = get_outputs();
	oldGains_ = new float[outputs.size()];
	for(unsigned int n = 0; n < outputs.size(); ++n){
		oldGains_[n] = 0.0f;
	}
	assert(get_inputs().size() > 0);
}

void AmpPanBehaviour::process(jack_nframes_t nframes){
	pos_.x = get_parameter_value("x");
	pos_.y = get_parameter_value("y");
	pos_.z = get_parameter_value("z");
	gain_ = get_parameter_value("gain");

	AudioStreamArray& inputs = get_inputs();
	float* in = inputs[0]->get_buffer()->get_buffer(); // ignore all others for now
	LoudspeakerArray& outputs = get_outputs();
	for(unsigned int o = 0; o < outputs.size(); ++o){
		Vec3 dPos = pos_ - outputs[o]->get_position();
		float D = dPos.mag();
		D = D < 1.0f ? 1.0f : D;
		// now use D
		float gCoef = 1.0f/(D*D) * gain_;
		// sum to buffer
		ab_sum_with_gain_linear_interp(in, outputs[o]->get_buffer()->get_buffer(), nframes, gCoef, oldGains_[o], 128);
		oldGains_[o] = gCoef;
	}
}
