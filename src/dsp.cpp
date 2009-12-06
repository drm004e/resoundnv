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
#include "resoundnv/dsp.hpp"
#include <iostream>

void LookupTable::print(){
	for(size_t n=0; n < size_; ++n){
		printf("%i %f\n",n,values_[n]);
	}
}

void test_dsp(){
	const int S = 20;
	std::cout << "Hann table\n";
	LookupTable* t1 = LookupTable::create_hann(S);
	t1->print();
	std::cout << "Cosine table\n";
	LookupTable* t2 = LookupTable::create_cosine(S);
	t2->print();
	std::cout << "Sine table\n";
	LookupTable* t3 = LookupTable::create_sine(S);
	t3->print();
	std::cout << "Blank table\n";
	LookupTable* t4 = LookupTable::create_empty(S);
	t4->print();
	delete t1;
	delete t2;
	delete t3;
	delete t4;
	
	t1 = LookupTable::create_hann(S);
	Phasor p1(S,4);
	printf("Phasor test %f %f \n",p1.get_freq(),p1.get_step() );
	for(int n=0; n < S; ++n){
		printf("%i %f %f\n",n,p1.get_phase(), t1->lookup_linear( p1.get_phase() ) );
		p1.tick();
	}
	Phasor p2(S*2,1);
	for(int n=0; n < S*2; ++n){
		printf("%i %f %f %f\n",n,p2.get_phase(), t1->lookup_linear( p2.get_phase()*S ), t1->lookup_linear( wrap(p2.get_phase()+0.5)*S ) );
		p2.tick();
	}
	delete t1;
}


