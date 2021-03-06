/*
 
Copyright (C) 2007 Lucas K. Wagner

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 
*/


#ifndef REPTATION_METHOD_H_INCLUDED
#define REPTATION_METHOD_H_INCLUDED

#include "Qmc_std.h"
#include "Qmc_method.h"
#include "Wavefunction.h"
#include "Wavefunction_data.h"
#include "Sample_point.h"
#include "Guiding_function.h"
#include "System.h"
#include "Pseudopotential.h"
#include "Split_sample.h"
#include "Space_warper.h"
class Program_options;
#include "Properties.h"
#include <deque>

//----------------------------------------------------------------------


struct Reptile_point {

  Reptile_point() { 
    age=0;
  }
  Properties_point prop;

  Array1 < Array1 <doublevar> > electronpos;

  doublevar age;
  doublevar branching; //!< branching weight

  //--------------------------------------------------
  void savePos(Sample_point * sample) {
    int nelectrons=sample->electronSize();
    electronpos.Resize(nelectrons);
    for(int i=0; i< nelectrons; i++) {
      electronpos(i).Resize(3);
      sample->getElectronPos(i,electronpos(i));
    }
  }
  //--------------------------------------------------

  void restorePos(Sample_point * sample) {
    int nelectrons=sample->electronSize();
    assert(nelectrons==electronpos.GetDim(0));
    for(int i=0; i< nelectrons; i++) 
      sample->setElectronPos(i,electronpos(i));
  }

  //--------------------------------------------------
  void write(string & indent, ostream & os) {
    int nelectrons=electronpos.GetDim(0);
    os << indent <<  "age " << age << endl;
    os << indent << "branching " << branching << endl;
	

    os << indent << "numElectrons " << nelectrons <<  endl;
    for(int e=0; e< nelectrons; e++) {
      os << indent;
      for(int d=0; d< 3; d++) 
        os << electronpos(e)(d) << "   ";
      os << endl;
    }

    string indent2=indent+"  ";
    os << indent << "Properties_point  {" << endl;
    prop.write(indent2,os);
    os << indent << "}" << endl;

  }
  //--------------------------------------------------
  void read(istream & is) { 
    string dummy; int nelectrons;
    const char *errmsg="misformatting in checkpoint read";
    
    is >> dummy >> age;
    if(dummy != "age") error(errmsg);
    is >> dummy >> branching;
    if(dummy != "branching") 
      error("expected branching, got ", dummy, " This probably means that your"
            " config files are too old.  Sorry!");
    is >> dummy >> nelectrons;
    if(dummy!="numElectrons") error(errmsg);
    
    electronpos.Resize(nelectrons);
    for(int e=0; e < nelectrons; e++) {
      read_array(is, 3, electronpos(e));
    }


    
    is >> dummy; 
    if(dummy!="Properties_point") error(errmsg);
    is >> dummy;
    prop.read(is);
    is >> dummy;
  }
  //--------------------------------------------------

  void mpiSend(int node) { 
    prop.mpiSend(node);
    MPI_Send(age,node);
    MPI_Send(branching,node);
    int nelectrons=electronpos.GetDim(0);
    MPI_Send(nelectrons, node);
    for(int e=0; e< nelectrons; e++) { 
      MPI_Send(electronpos(e),node);
    }
  }
  void mpiReceive(int node) { 
    prop.mpiReceive(node);
    MPI_Recv(age,node);
    MPI_Recv(branching,node);
    int nelectrons;
    MPI_Recv(nelectrons,node);
    electronpos.Resize(nelectrons);
    for(int e=0; e< nelectrons; e++) { 
      MPI_Recv(electronpos(e),node);
    }
  }
};
 
//######################################################################

class Reptile { 
  public:
  void mpiSend(int node);
  void mpiReceive(int node);
  void read(istream & is);
  void write(ostream & os);
  deque <Reptile_point> reptile; 
  int direction;
};

/*!
\brief
Evaluates the expectation value \f$ <\Psi|H|\Psi>/<\Psi|\Psi> \f$
stochastically.  Keyword: REPTATION 
*/
class Reptation_method : public Qmc_avg_method
{
public:

  Reptation_method() {
    have_read_options=0;
    have_generated_variables=0;
    have_attached_variables=0;
    sys=NULL;
    mywfdata=NULL;
    guidewf=NULL;
    pseudo=NULL;
    wf=NULL;
  }
  void read(vector <string> words,
            unsigned int & pos,
            Program_options & options);
  void run(Program_options & options, ostream & output);
  
  int showinfo(ostream & os);

  int generateVariables(Program_options & options);

  virtual void runWithVariables(Properties_manager & prop, 
                                System * sys,
                                Wavefunction_data * wfdata,
                                Pseudopotential * psp,
                                ostream & output);

  ~Reptation_method()
  {
    
    if(have_generated_variables) {
      if(sys) delete sys;
      if(mywfdata) delete mywfdata;
      if(pseudo) delete pseudo;
    }

    if(guidewf) delete guidewf;

    for(int i=0; i< densplt.GetDim(0); i++) {
      if(densplt(i)) delete densplt(i);
    }
    
   }

private:
  int allocateIntermediateVariables(System * , Wavefunction_data *);
  int deallocateIntermediateVariables();
  
  int readcheck(string & filename, Array1 <Reptile> & reptiles);
  void storecheck(Array1<Reptile> & reptiles, string & filename);

  doublevar getAcceptance(deque <Reptile_point> & reptile,
                          Reptile_point & pt, int direction);
  doublevar slither(int direction, 
                    deque <Reptile_point> & reptile,
                    Properties_gather & mygather, 
                    Reptile_point & pt,
                    doublevar & main_diffusion);

  void get_avg(deque <Reptile_point> & reptile, 
	       Properties_point & pt);
  void get_center_avg(deque <Reptile_point> & reptile, 
		      Properties_point & pt);
  int have_read_options;
  int have_generated_variables;
  int have_attached_variables;

  string center_trace;
  int trace_wait;

  
  doublevar energy_cutoff; //!< when to cut off the branching term
  doublevar eref; //!< reference energy
  bool print_pdb;



  int nblock;
  int nstep;
  string readconfig;
  string log_label;
  string storeconfig;
  doublevar timestep;
  int reptile_length;

  Properties_gather mygather;
  Dynamics_generator * sampler;
  Guiding_function * guidewf;
  Pseudopotential * pseudo;
  System * sys;
  Sample_point *  sample; 
  Wavefunction * wf; 
  Wavefunction_data * mywfdata;


  Array1 < Local_density_accumulator *> densplt;
  vector <vector <string> > dens_words;
  Array1 < Average_generator * > average_var;
  vector <vector <string> > avg_words;

  
};

#endif //REPTATION_METHOD_H_INCLUDED
//------------------------------------------------------------------------
