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
#include <cmath>

const float PI=3.14159;
const float TWOPI=2.0f*PI;
const float HALFPI=0.5f*PI;

class LookupTable{
private:
	float* values_;
	size_t size_;
public:
	LookupTable():values_(0){}
	LookupTable(size_t size) : size_(size+1), values_(new float[size_]) {} // a guard point is added to make interp easier
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
	static LookupTable* create_empty(size_t size){
		Table* t = new Table(size);
		std::memset(t->values_,0,sizeof(float)*(size+1));
	}
	static LookupTable* create_sine(size_t size){
		float rSize = 1.0f/(float)size;
		Table* t = new Table(size); 
		for(int n=0; n<size; ++n){
			t->values_[n] = std::sin(TWOPI * n * rSize);
		}
		t->values_[size+1] = t->values_[size]; // guard
	}
	static LookupTable* create_cosine(size_t size){
		float rSize = 1.0f/(float)size;
		Table* t = new Table(size);
		for(int n=0; n<size; ++n){
			t->values_[n] = std::cos(TWOPI * n * rSize);
		}
		t->values_[size+1] = t->values_[size]; // guard
	}
	static LookupTable* create_hann(size_t size){
		float rSize = 1.0f/(float)size;
		Table* t = new Table(size);
		for(int n=0; n<size; ++n){
			t->values_[n] = 0.5f + (1.0f - std::cos(TWOPI * n / rSize-1);
		}
		t->values_[size+1] = t->values_[size]; // guard
	}
};

class Phasor{
private:
	float step_; // step per sample calculated when setting frequency
	float phase_; // current value and therefore phase between 0 and 1
	float freq_; // the last frequency set
	float SR_; // sample rate
public:
	Phasor(float SR, float phase, float freq) : SR_(SR), phase_(phase) { set_freq(freq_); }
	float get_phase() { return phase_; }
	void tick(){ 
		phase_ += step_; 
		if(phase_>1.0f){phase_ -= 1.0f; return;}
		if(phase_<0.0f){phase_ += 1.0f; return;}
	}
	void set_freq(float freq){ 
		freq_ = freq;
		step_ = freq_/SR;
	}
};

