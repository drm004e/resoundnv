//    Resound
//    Copyright 2007 David Moore and James Mooney
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

#include <cmath>
#include <iostream>
#include <jack/jack.h>
#include <vector>
#include <list>
#include <string>
#include <cassert>
#include <boost/program_options.hpp>
#include <fstream>
#include <iomanip>

const double PI=3.14159265;


// should put in command line arguments for:
// -c --capture capture port name
// -g --gain impulse gain
// -t --theshold
// -T --tminus countdown so you can get out of the room
// -m --mode single/multi mic modes so a test if you have three mics or a test with one mode for getting latency measurement
// -l --latency compensation

// these globals are for the command line options
enum RESOUND_CAL_MODE{
	RCM_SINGLE,
	RCM_MULTI	
};
std::string g_capturePortName;
double g_impulseGain;
double g_threshold;
int g_tminus; // in seconds
RESOUND_CAL_MODE g_mode;
int g_latencyCompensation; // in samples

struct Vec3{
	double x,y,z;
	Vec3(){}
	Vec3(double a, double b, double c) : x(a), y(b), z(c){}
	virtual ~Vec3(){}
	double mag() const {
		return sqrt(x*x + y*y + z*z);
	}	
};

double distance(const Vec3& a, const Vec3& b){
	
	Vec3 d(b.x-a.x, b.y-a.y, b.z-a.z);
	return d.mag();
}

double rad2deg(double v){
	return v * 180./PI;
}
double deg2rad(double v){
	return v * PI/180.;
}
inline double sqr(double x) {return x*x;}
bool triangulate(Vec3& result, double r1, double r2, double r3, double tol){
	double x,y,z;
	x = (sqr(r1) - sqr(r2) + 1.0)/2.0;
	y = (sqr(r1) - sqr(r3) + 2.0) / 2.0 - x;
	z = sqrt( sqr(r1) - sqr(x) - sqr(y) );
	result.x = x;
	result.y = y;
	result.z = z;
	return true;
}

/// class for peak detection on a sequence of samples
class PeakFinder{
	float m_peak;
	int m_counter;
	int m_index;
	bool m_found;
	static const float threshold=0.2f;
public:
	PeakFinder() : m_peak(0.0f), m_index(-1), m_counter(0), m_found(false) {}
	void test_sample(float v){
		if(v > threshold && !m_found){ 
			m_peak = v;
			m_index = m_counter;
			m_found=true;
		}
		++m_counter;
	}
	float get_peak(){return m_peak;}
	int get_index(){return m_index;}
	void reset(){m_peak = 0.0f; m_index=-1; m_counter=0; m_found=false;}
};

class CalibrationException : public std::exception{
};

// a calibration test will connect and disconnect to jack automatically
// the connection will only be open for the duration of the test
class CalibrationTest{
protected:
	jack_client_t* m_jc; ///< the jack client
	jack_port_t* m_outputPort; ///< the input port
	jack_port_t* m_inputPort; ///< the output port
	int m_testSize; ///< the length of the test is samples
	int m_testPos; ///< the current position of the test
	bool m_testIsRunning;
	double m_SR; ///< sample rate
public:
	/// configure the test in the consructor
	CalibrationTest() : 
		m_testIsRunning(false){
	}
	virtual ~CalibrationTest(){
	}
	// complete run of test here
	// instanciates jack and ports then runs the test and deactivates jack
	virtual void run_test(std::string conInput, std::string conOutput, int tSamples){
		std::cout << "{";
		m_jc = jack_client_open("resoundnv_cal",JackNullOption,0);
		if(!m_jc) throw CalibrationException();
		m_outputPort = jack_port_register(m_jc,"output",JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput,0);
		m_inputPort = jack_port_register(m_jc,"input",JACK_DEFAULT_AUDIO_TYPE,JackPortIsInput,0);
		jack_set_process_callback(m_jc,CalibrationTest::test_callback,this);
		jack_activate(m_jc);
		jack_connect(m_jc,"resoundnv_cal:output",conOutput.c_str());
		jack_connect(m_jc,conInput.c_str(),"resoundnv_cal:input");
		m_SR=jack_get_sample_rate(m_jc);
		m_testSize = int(tSamples);
		m_testPos = 0;
		
		m_testIsRunning = true;
		usleep(50000);
		jack_deactivate(m_jc);
		if(m_jc) jack_client_close(m_jc);
		m_jc=0;
		std::cout << "}";
	}

	virtual int process(jack_nframes_t nframes) = 0;

	static int test_callback(jack_nframes_t nframes, void *arg){
		CalibrationTest* t = (CalibrationTest*)arg;
		if(t->m_testPos++ < t->m_testSize && t->m_testIsRunning){
			return t->process(nframes);
		} else {
			t->m_testIsRunning = false;
			return 0;
		}
	}
	
};

class ImpulseCalibrator : public CalibrationTest{
	bool m_impulseOnOne;
public:
	PeakFinder m_pf;
	virtual void run_test(std::string conInput, std::string conOutput, int tSeconds){
		//std::cout << "ImpulseCalibrator: ";
		m_impulseOnOne=true;
		m_pf.reset();
		CalibrationTest::run_test(conInput,conOutput,tSeconds);
	}
	virtual int process(jack_nframes_t nframes){
		float* out = (float*)jack_port_get_buffer(m_outputPort,nframes);
		float* in = (float*)jack_port_get_buffer(m_inputPort,nframes);
		while(nframes-- > 0){
			if(m_impulseOnOne){
				*out++ = 1.0f;
				m_impulseOnOne=false;
			} else {
				*out++ = 0.0f;
			}
			m_pf.test_sample(fabs(*in++));
		}
		
		return 0;
	}	
};

struct LoudspeakerCal{
	LoudspeakerCal(){}
	std::string name; ///< its name for xml id
	std::string portName; ///< the port it was found on
	double distance[3]; ///< results from mic positions
	Vec3 pos; ///< calculated final position
	Vec3 rot; ///< calculated final orientation vector normalised and asumed to point diectly at origin
};

bool parse(int argc, char** argv){

	namespace po = boost::program_options;
	// Declare the supported options.
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("capture,c", po::value<std::string>()->default_value("system:capture_1"), "set the capture port for the calibration mic")
		("gain,g", po::value<double>()->default_value(1.0), "set the level of the impulse")
		("threshold,t", po::value<double>()->default_value(0.1), "set the level of the detection threshold")
		("tminus,T", po::value<int>()->default_value(0), "set the countdown timer in seconds")
		("mode,m", po::value<int>()->default_value(0), "set the mode 0=single 1=multi")
		("latency,l", po::value<int>()->default_value(384), "set the latency compensation in samples")
	;
	
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);    
	
	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}

	if (vm.count("capture")) {
		g_capturePortName = vm["capture"].as<std::string>();
	}

	if (vm.count("gain")) {
		g_impulseGain = vm["gain"].as<double>();
	}

	if (vm.count("threshold")) {
		g_threshold = vm["threshold"].as<double>();
	}
	if (vm.count("tminus")) {
		g_tminus = vm["tminus"].as<int>();
	}
	if (vm.count("mode")) {
		g_mode = (RESOUND_CAL_MODE)vm["mode"].as<int>();
	}
	if (vm.count("latency")) {
		g_latencyCompensation = vm["latency"].as<int>();
	}
	return 0;
}
int main(int argc, char** argv){
	// specify input port for cal mic
	// process will go through all outputs that go to a sound card
	// setup for each calibrartion point with clear instructions
	// run burst calibration gathering results
	// after all three calibrations bursts perform analysis

	// parse command line
	if(parse(argc,argv)) return 1;

	// quickly open jack and determine ports available
	std::vector<std::string> portList;	
	jack_client_t* jc = jack_client_open("resoundnv_cal",JackNullOption,0);
	if(!jc) exit(1);
	const char** ports = jack_get_ports(jc,"","",JackPortIsInput|JackPortIsPhysical); // filter to find all the pysical outgoing ports
	if(ports){	
		for(int n = 0; ports[n] != 0; n++){
			std::string portName(ports[n]);
			std::cout << "Port found: " << portName << std::endl;
			portList.push_back(portName);
			
		}
		free(ports);
	}
	if(jc) jack_client_close(jc);

	// now do the calibration	
	int N_SPEAKERS = portList.size() > 8 ? 8 : portList.size();
	LoudspeakerCal* speaker = new LoudspeakerCal[N_SPEAKERS];
	for(int micPos = 0; micPos < 3; micPos++){
		std::cout << "Ready\nPress a key to proceed with mic position "<<micPos<<"\n******************************************************\n* WARNING very loud noise bursts will be played back *\n******************************************************\n";
		getchar();

		if(g_tminus > 0){
			for(int n = g_tminus; n > 0; n--){
				std::cout << "Impulse burst in " << n << " seconds\n";
				sleep(1);
			}
		}

		for(int n = 0; n < N_SPEAKERS; n++){
			const int numRuns = 5;
			double D= 0.;
			double sumD= 0.;
			for(int run = 0; run < numRuns; ++run){ // run averaging
				ImpulseCalibrator ipc[3];
				ipc[micPos].run_test(g_capturePortName.c_str(),portList[n].c_str(),128*32);
				int s = ipc[micPos].m_pf.get_index() - g_latencyCompensation;
				float peak = ipc[micPos].m_pf.get_peak();
				if(peak >= g_threshold){
					std::cout << "Impulse detected from " << portList[n].c_str() << " at index=" << ipc[micPos].m_pf.get_index()<<" ~"<< s <<" level=" << peak << std::endl;
					const double SR = 44100.;
					const double SOS = 340.29;
					D = double(s) * 1./SR * SOS;
					sumD += D;
					std::cout << "Distance=" << D << " m\n";
				}
				usleep(10000);
			}
			speaker[n].distance[micPos] = sumD / double(numRuns);
			std::cout << "Avg Distance=" << speaker[n].distance[micPos] << " m\n";
			usleep(500000);
		}
		
	}

	std::ofstream f("calibration.xml");
	f << std::setprecision(2) << "<resound>\n";
	for(int n = 0; n < N_SPEAKERS; n++){
		if(triangulate(speaker[n].pos, speaker[n].distance[0],speaker[n].distance[1],speaker[n].distance[2], 0.001)){
			std::cout << std::setprecision(2) << "Match found for " << portList[n].c_str() << " x=" << speaker[n].pos.x << " y=" << speaker[n].pos.y << " z=" << speaker[n].pos.z <<std::endl;
			f << "<loudspeaker id=\"loudspeaker"<<n<<"\" port=\""<<portList[n].c_str()<<"\" x=\""<<speaker[n].pos.x<<"\" z=\""<<speaker[n].pos.y<<"\" y=\""<<speaker[n].pos.z<<"\"/>\n";
		} else {
			std::cout << "No match for " << portList[n].c_str() << std::endl;
		}
	}
	f << "</resound>\n";	
	
}
