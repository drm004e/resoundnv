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

void BRouteSet::create_route(const BufferRef& a, const BufferRef& b, float gain){
    if( a.isBus ) { throw Exception("Route source is not a behaviour output."); }
    if( !b.isBus ) { throw Exception("Route destination is not a bus."); }
    std::cout << "Found route 1 to 1 : " << a.id << " to " << b.id << std::endl;
    routes_.push_back(BRoute(a.buffer,b.buffer, gain ));
}

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
				// get the from and to attributes
				ObjectId from = get_attribute_string(child,"from");
				ObjectId to = get_attribute_string(child,"to");
				float gain = get_optional_attribute_float(child,"gain", 1.0);

                                BufferRefVector fromRefs = SESSION().lookup_buffer(from);
                                BufferRefVector toRefs = SESSION().lookup_buffer(to);

                                int fromSize = fromRefs.size();
                                int toSize = toRefs.size();
                                if(fromSize == 0 || toSize==0){
                                    throw Exception("Invalid route.");
                                }
                                if(fromSize == 1 && toSize == 1){
                                    create_route(fromRefs[0], toRefs[0], gain);
                                } else if (fromSize == 1 && toSize > 1){
                                    // one to many
                                    for(int n = 0; n < toSize; ++n){
                                        create_route(fromRefs[0], toRefs[n], gain);
                                    }
                                } else if (fromSize > 1 && toSize == 1){
                                    // many to one
                                    for(int n = 0; n < fromSize; ++n){
                                        create_route(fromRefs[n], toRefs[0], gain);
                                    }
                                } else if (fromSize > 1 && toSize > 1){
                                    // many to many
                                    for(int n = 0; n < fromSize; ++n){
                                        ObjectId fromName = fromRefs[n].id.substr(fromRefs[n].id.find('.'));
                                        for(int m = 0; m < toSize; ++m){    
                                            ObjectId toName = toRefs[m].id.substr(toRefs[m].id.find('.'));
                                            if(fromName == toName){
                                                create_route(fromRefs[n], toRefs[m], gain);
                                            }
                                        }
                                    }
                                }

			}
		}
	}
}

BParam::BParam(float& v, float startingValue) : value_(v){
	value_ = startingValue; // remember that this is a reference
}
void BParam::init_from_xml(const xmlpp::Element* nodeElement){
	addr_ = get_optional_attribute_string(nodeElement,"address");
	value_ = get_optional_attribute_float(nodeElement,"value");
	// at this point we should register the parameter address with osc
	if(addr_ != ""){
		SESSION().add_method(addr_,"f", BParam::lo_cb_params, this);
	}
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
				ObjectId id = get_attribute_string(child,"id");
				BParamMap::iterator it = params_.find(id);
				if(it != params_.end()){
					it->second->init_from_xml(child);
				} else {
					std::stringstream str;
					str << "A parameter with id=\""<<id<<"\" is not registered for this behaviour.";
					throw Exception(str.str().c_str());
				}
			}
		}
	}
	DynamicObject::init_from_xml(nodeElement);
}


void Behaviour::register_parameter(ObjectId id, BParam* param){
	BParamMap::iterator it = params_.find(id);
	if(it == params_.end()){
		params_[id] = param;

	} else {
		throw Exception("Cannot register parameter, non-unique parameter name.");
	}
}

AudioBuffer* Behaviour::create_buffer(ObjectId subId, ObjectId forceId){
// TODO: nasty joink here, forcing ids is not that nice but is sometimes nessersary
// consider a redesign.
    AudioBuffer* b = new AudioBuffer();
    jack_nframes_t s = SESSION().get_buffer_size();
    b->allocate(s);
    BufferRef ref;
    std::stringstream str;
    ObjectId id = get_id(); // this may be "" if the behaviour has not yet initialised from xml
    if(forceId != "") id = forceId; // in which case we may be able to force.
    if(subId != ""){
        str << id << "." << subId;
    } else {
        str << id << "." << buffers_.size();
    }
    ref.id =  str.str();
    ref.isAlias = false;
    ref.isBus = false;
    ref.creator = id;
    ref.buffer = b;
    SESSION().register_buffer(ref);
    buffers_.push_back(b);
    return b;
}

// ---------------------------------------
Diskstream::Diskstream() :
		ringBuffer_(0),
		diskBuffer_(0),
		copyBuffer_(0),
		playing_(true)
{}

void Diskstream::init_from_xml(const xmlpp::Element* nodeElement){
	// diskstream maintains a jack ring buffer and two process functions are called from seperate threads
	// create a ring buffer
	// open the file for reading
	// check channels and assert mono
	// read as much as possible into the ring buffer

	path_ = get_attribute_string(nodeElement,"source");
        gain_ = get_optional_attribute_float(nodeElement,"gain", 1.0);

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

	Behaviour::init_from_xml(nodeElement);

        create_buffer();
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
		ab_copy_with_gain(copyBuffer_, get_buffer(0).get_buffer(),nframes, gain_);
		//size_t bytesRead = jack_ringbuffer_read (ringBuffer_, (char*)tbuffer, bytesToRead);
		//TODO gain should be applied here
		//printf("Buffer read %i bytes, from %i available\n",bytesRead, rSpace);
	} else {
		// buffer underrun
		printf("Buffer underrun!\n");
	}
	//avg_signal_in_buffer(get_buffer()->get_buffer(),nframes);
	//avg_signal_in_buffer(tbuffer,nframes);
	//get_vu_meter().analyse_buffer(get_buffer(0).get_buffer(),nframes);

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

Livestream::Livestream(){}

void Livestream::init_from_xml(const xmlpp::Element* nodeElement){
	// this node will need to establish a jack stream via the jack system, it should kill the port when done
	connectionName_ = get_attribute_string(nodeElement,"port");
	ObjectId id = get_attribute_string(nodeElement,"id");
        gain_ = get_optional_attribute_float(nodeElement,"gain", 1.0);
	port_ = new JackPort(id, JackPortIsInput ,&SESSION());
	port_->connect(connectionName_);
	std::cout << "Livestream " << id << std::endl;

	Behaviour::init_from_xml(nodeElement);
        create_buffer();
}
void Livestream::process(jack_nframes_t nframes){
	// copy from jack buffer applying gain
	ab_copy_with_gain(port_->get_audio_buffer(nframes), get_buffer(0).get_buffer(),nframes, gain_);

	//avg_signal_in_buffer(get_buffer()->get_buffer(),nframes); // sound tested here

	//TODO optional vumetering
	//get_vu_meter().analyse_buffer(get_buffer(0).get_buffer(),nframes);
};
// --------------------------------------

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

IOHelper::IOHelper(){}
void IOHelper::init_from_xml(const xmlpp::Element* nodeElement){
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
				BufferRefVector v = SESSION().lookup_buffer(id);
                                if(v.size() == 1){
                                    inputs_.push_back(v[0].buffer);
                                } else {
                                    throw Exception("IOHelper <input> tags must refer to single buffers");
                                }

			} else if(name=="output"){
				ObjectId id = get_attribute_string(child,"ref");
				Loudspeaker* loudspeaker = SESSION().resolve_loudspeaker(id);
				outputs_.push_back(loudspeaker);
			}
		}
	}
}

void AttBehaviour::process(jack_nframes_t nframes){
	// ok we would get all the routes for the first routeset
	// then we do buffer copy for each one applying our current gain setting
	float level = level_;
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

MultipointCrossfadeBehaviour::MultipointCrossfadeBehaviour(){
	register_parameter("position",new BParam(position_,0.0f));
	register_parameter("gain",new BParam(gain_,0.0f));
	register_parameter("slope",new BParam(slope_,0.0f));
}

void MultipointCrossfadeBehaviour::init_from_xml(const xmlpp::Element* nodeElement){

	std::cout << "Created MultipointCrossfade routeset behaviour object!" << std::endl;
	// right much work for this algorithm is based on the concept of scaling, offseting and clipping the range 0 - 1
	// see pd patch phasor-chase-algorithm.pd

	// OSC BParams for fader "position" 0 - 1, gain factor "gain" and "slope", slope controls the mapping function

	// Algorithm uses scales and offsets the position range such that we get a set of 0-1 indexs for each of the routesets
	// the indexs are used to table lookup into a hanning window function which is then applied modified by apropriate factors.


	hannFunction = LookupTable::create_hann(HANN_TABLE_SIZE);
	RouteSetBehaviour::init_from_xml(nodeElement);	
}
void MultipointCrossfadeBehaviour::process(jack_nframes_t nframes){



	BRouteSetArray& routeSets = get_route_sets();
	int numRoutes = routeSets.size();
	float N = (float)numRoutes;
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
ChaseBehaviour::ChaseBehaviour() : phasor(44100.0f/128.0f,1){
	register_parameter("freq",new BParam(freq_,1.0f));
	register_parameter("phase",new BParam(phase_,0.0f));
	register_parameter("gain",new BParam(gain_,0.0));
	register_parameter("slope",new BParam(slope_,1.5));
	hannFunction = LookupTable::create_hann(HANN_TABLE_SIZE);
}

void ChaseBehaviour::init_from_xml(const xmlpp::Element* nodeElement){
	std::cout << "Created ChaseBehaviour routeset behaviour object!" << std::endl;
	// this is based on the multipoint crossfader but uses a phasor to control position
	RouteSetBehaviour::init_from_xml(nodeElement);
	phasor.set_phase(phase_);
}
void ChaseBehaviour::process(jack_nframes_t nframes){

	BRouteSetArray& routeSets = get_route_sets();
	int numRoutes = routeSets.size();
	float N = (float)numRoutes;

	phasor.set_freq(freq_);

	// the chase gets its position from the phasor
	float phase = clip(phasor.get_phase(),0,1);
	phasor.tick();

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

AmpPanBehaviour::AmpPanBehaviour(){
	register_parameter("x",new BParam(pos_.x,0));
	register_parameter("y",new BParam(pos_.y,0));
	register_parameter("z",new BParam(pos_.z,0));
	register_parameter("gain",new BParam(gain_,0));
}

void AmpPanBehaviour::init_from_xml(const xmlpp::Element* nodeElement){
	std::cout << "Created AmpPan io based behaviour object!" << std::endl;
	io_.init_from_xml(nodeElement);

	// identify the number off outputs and get each ones gain
	// setup storage for previous gain suitable for interpolation
	IOHelper::LoudspeakerArray& outputs = io_.get_outputs();
	oldGains_ = new float[outputs.size()];
	for(unsigned int n = 0; n < outputs.size(); ++n){
		oldGains_[n] = 0.0f;
	}
	assert(io_.get_inputs().size() > 0);
	
	Behaviour::init_from_xml(nodeElement);
}

void AmpPanBehaviour::process(jack_nframes_t nframes){

	IOHelper::BufferArray& inputs = io_.get_inputs();
	float* in = inputs[0]->get_buffer(); // ignore all others for now
	IOHelper::LoudspeakerArray& outputs = io_.get_outputs();
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


GainInsertBehaviour::GainInsertBehaviour(){
	register_parameter("gain",new BParam(gain_,1.0));
}

void GainInsertBehaviour::init_from_xml(const xmlpp::Element* nodeElement){

	std::cout << "Created Gain Insert Behaviour " << std::endl;
	io_.init_from_xml(nodeElement);
	
        int chans = io_.get_inputs().size();
	assert(chans > 0);
        for(int n = 0; n < chans; ++n){
            create_buffer();
        }
        Behaviour::init_from_xml(nodeElement);
}

void GainInsertBehaviour::process(jack_nframes_t nframes){

        IOHelper::BufferArray& inputs = io_.get_inputs();
        int chans = inputs.size();
        for(int chan = 0; chan < chans; ++chan){
           
            float* in = inputs[0]->get_buffer(); // ignore all others for now
            float* out = get_buffer(chan).get_buffer();
            ab_copy_with_gain(in, out, nframes, gain_);
        }
}

RingmodInsertBehaviour::RingmodInsertBehaviour(): phasor_(44100.0f,220){
	register_parameter("freq",new BParam(freq_,220));
	register_parameter("gain",new BParam(gain_,1.0));
	sinFunction_ = LookupTable::create_sine(SIN_TABLE_SIZE);
}

void RingmodInsertBehaviour::init_from_xml(const xmlpp::Element* nodeElement){


        
	io_.init_from_xml(nodeElement);
	std::cout << "Created Rindmod Insert Behaviour " << std::endl;

        int chans = io_.get_inputs().size();
	assert(chans > 0);
        for(int n = 0; n < chans; ++n){
            create_buffer();
        }
	Behaviour::init_from_xml(nodeElement);
}

void RingmodInsertBehaviour::process(jack_nframes_t nframes){

        phasor_.set_freq(freq_);
        
        IOHelper::BufferArray& inputs = io_.get_inputs();
        int chans = inputs.size();
        
        float phase = phasor_.get_phase();
        
        for(int chan = 0; chan < chans; ++chan){
            phasor_.set_phase(phase);

            float* in = inputs[0]->get_buffer(); // ignore all others for now
            float* out = get_buffer(chan).get_buffer();
            for(int n = 0; n < nframes; ++n){
                float osc = sinFunction_->lookup_linear(phasor_.get_phase() * (float)SIN_TABLE_SIZE );
                out[n] = in[n] * osc * gain_;
                phasor_.tick();
            }
            
        }
}

// --------------
LADSPABehaviour::LADSPABehaviour(){}

void LADSPABehaviour::init_from_xml(const xmlpp::Element* nodeElement){
	std::cout << "Created LDSPA Behaviour " << std::endl; 
	io_.init_from_xml(nodeElement);
	ObjectId id = get_attribute_string(nodeElement,"id");
	
	plugName_ = get_attribute_string(nodeElement,"plug");
	descriptor_ = SESSION().get_ladspa_host().get_descriptor(plugName_);
	instance_ = descriptor_->instantiate(descriptor_, 44100);
	int inCount = 0;
	int outCount = 0;
	int controlCount = 0;
	for(int n = 0; n < descriptor_->PortCount;++n){
		int pd = descriptor_->PortDescriptors[n];
		if(pd & LADSPA_PORT_AUDIO && pd & LADSPA_PORT_INPUT){
			std::cout << "Audio Input Port: " << descriptor_->PortNames[n] << std::endl;
			// should tally this with the first input we find and connect
			if (inCount < io_.get_inputs().size() ){
				float* inBuffer = io_.get_inputs()[inCount]->get_buffer();
				descriptor_->connect_port(instance_,n,inBuffer);
				++inCount;
			} else {
				throw Exception("Not enough inputs specified for LADSPA plugin.");
			}
			
		} else if(pd & LADSPA_PORT_AUDIO && pd & LADSPA_PORT_OUTPUT){
			std::cout << "Audio Output Port: " << descriptor_->PortNames[n] << std::endl;
			// should create an output buffer and connect
			AudioBuffer* b = create_buffer("",id); // TODO: nasty joink here, has to force the id because the behaviour id is not yet found in base class.
			descriptor_->connect_port(instance_,n,b->get_buffer());
			++outCount;
		} else if(pd & LADSPA_PORT_CONTROL && pd & LADSPA_PORT_INPUT){
			descriptor_->PortRangeHints[n];
			std::cout << "Control Input Port: " 
				<< descriptor_->PortNames[n] 
				<< " (" << descriptor_->PortRangeHints[n].LowerBound
				<< " : "<< descriptor_->PortRangeHints[n].UpperBound << " )" << std::endl;
			// create a parameter for this
			controlPortValues_.push_back(new float(0.0));
			float* v = controlPortValues_[controlCount];
			register_parameter(descriptor_->PortNames[n],new BParam(*v,0.0f));
			descriptor_->connect_port(instance_,n,v);
			++controlCount;
		} else {
			std::cout << "Unsual Port: "<< descriptor_->PortNames[n] << std::endl;
			throw Exception("LADSPA Unusual port.");
		}
	}
	Behaviour::init_from_xml(nodeElement);
	// activate after fully initialised
	descriptor_->activate(instance_);
}

void LADSPABehaviour::process(jack_nframes_t nframes){
	descriptor_->run(instance_, nframes);
}




