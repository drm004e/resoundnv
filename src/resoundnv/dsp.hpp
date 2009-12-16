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
#include <cassert>
#include <cstring>
#include <cmath>

const float PI=3.14159;
const float TWOPI=2.0f*PI;
const float HALFPI=0.5f*PI;

class LookupTable{
private:
	size_t size_;
	float* values_;
public:
	LookupTable(size_t size) : size_(size+1), values_(new float[size+1]) {} // a guard point is added to make interp easier
	~LookupTable(){ if(values_) delete[] values_; }
	float lookup(float index){ return values_[(int)index]; }
	float lookup_linear(float index) {
		// assume a guard point
		float i,f;
		f = std::modf(index, &i);
		float l = values_[(int)i];
		float r = values_[(int)i+1];
		return (r-l)*f + l;
	}

	/// testing print code
	void print();

	static LookupTable* create_empty(size_t size){
		LookupTable* t = new LookupTable(size);
		std::memset(t->values_,0,sizeof(float)*(size+1));
		return t;
	}
	static LookupTable* create_sine(size_t size){
		float rSize = 1.0f/(float)size;
		LookupTable* t = new LookupTable(size); 
		for(size_t n=0; n<size; ++n){
			t->values_[n] = std::sin(TWOPI * n * rSize);
		}
		t->values_[size] = t->values_[size-1]; // guard
		return t;
	}
	static LookupTable* create_cosine(size_t size){
		float rSize = 1.0f/(float)size;
		LookupTable* t = new LookupTable(size);
		for(size_t  n=0; n<size; ++n){
			t->values_[n] = std::cos(TWOPI * n * rSize);
		}
		t->values_[size] = t->values_[size-1]; // guard
		return t;
	}
	static LookupTable* create_hann(size_t size){
		float rSize = 1.0f/(float)size;
		LookupTable* t = new LookupTable(size);
		for(size_t n=0; n<size; ++n){
			t->values_[n] = 0.5f * (1.0f - std::cos(TWOPI * n / (rSize-1)));
		}
		t->values_[size] = t->values_[size-1]; // guard
		return t;
	}
};

class Phasor{
private:
	float SR_; // sample rate
	float step_; // step per sample calculated when setting frequency
	float freq_; // the last frequency set
	float phase_; // current value and therefore phase between 0 and 1
public:
	Phasor(float SR,  float freq, float phase=0.0f) : SR_(SR), phase_(phase) { set_freq(freq); }
	float get_step() { return step_; }
	float get_freq() { return freq_; }
	float get_phase() { return phase_; }
	void tick(){ 
		phase_ += step_; 
		if(phase_>=1.0f){phase_ -= 1.0f; return;}
		if(phase_<0.0f){phase_ += 1.0f;}
	}
	void set_freq(float freq){
		assert(freq <= SR_); // beacause this would cause problems in the tick code
		freq_ = freq;
		step_ = freq_/SR_;
	}
	void set_phase(float phase){
		phase_ = phase;
	}
};

/// a vu metering class
class VUMeter{
private:
	int size_;
	float rms_;
	float peak_;
	float margin_;
	float sumOfSquares_;
	int count_;
public:
	VUMeter(int size=4096) : //TODO the magic number here for buffer size implies a maximal jack buffer size of 4096
		size_(size), 
		rms_(0.0f), 
		peak_(0.0f), 
		margin_(0.0f), 
		sumOfSquares_(0.0f), 
		count_(0){}
	void analyse_buffer(float* buffer, int N){
		for(int n=0; n<N; ++n){
			float v = buffer[n];
			float magv = std::abs(v);
			sumOfSquares_ += v*v;
			peak_ = peak_ < magv ? magv : peak_;
			peak_ *= 0.9999; /// peak falloff
			margin_ = margin_ < magv ? magv : margin_;
			++count_;
		}
		if(count_ >= size_){
			rms_ = std::sqrt(sumOfSquares_);
			sumOfSquares_ = 0.0f;
		}
	}
	float get_rms() const { return rms_; }
	float get_peak() const { return peak_; }
	float get_margin() const { return margin_; }
	void  reset_margin(){ margin_=0.0f; }
};

/// a function to perform wrap around on a floating point value
/// wraps the input - very useful with phasors
inline float wrap(float op){return std::fmod(op,1.0f);}

// make a signal zero if it is less than a low bound or greater than an upper bound
// again this useful with phasors
inline float zero_outside_bounds(float v, float lower, float upper){
	if(v < lower) return 0.0f;
	if(v > upper) return 0.0f;
	return v;
}
/// clipping function
inline float clip(float v, float lower, float upper){
	if(v < lower) return lower;
	if(v > upper) return upper;
	return v;
}

/// some testing code
void test_dsp();
