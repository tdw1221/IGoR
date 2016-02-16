/*
 * GenModel.h
 *
 *  Created on: 3 nov. 2014
 *      Author: marcou
 *      This class designs a generative model and supply all the methods to run a maximum likelihood estimate of the generative model
 */

#ifndef GENMODEL_H_
#define GENMODEL_H_

#include "Model_Parms.h"
#include "Model_marginals.h"
#include "Errorrate.h"
#include "Rec_Event.h"
#include "Utils.h"
#include <list>
#include <string>
#include <random>
#include <chrono>
#include <fstream>
#include <omp.h>
#include <stdexcept>
#include <stack>


class GenModel {
public:
	GenModel(const Model_Parms&);
	GenModel(const Model_Parms& , const Model_marginals&);
	//TODO: add all the necessary constructors: with just model_parms, with model_parms and marginals
	virtual ~GenModel();
	bool infer_model(const std::vector<std::pair<std::string,std::unordered_map<Gene_class , std::vector<Alignment_data>>>>& sequences ,const  int iterations ,const std::string path , double likelihood_threshold=1e-25 , double proba_threshold_factor=0.001 , double mean_number_seq_err_thresh = INFINITY);
	std::forward_list<std::pair<std::string , std::queue<std::queue<int>>>> generate_sequences (int,bool);
	void generate_sequences(int,bool,std::string,std::string);
	bool load_genmodel();
	bool write2txt ();
	bool readtxt ();
	void write_seq2txt(std::string,std::forward_list<std::string>);
	void write_seq_real2txt(std::string , std::string , std::forward_list<std::pair<std::string , std::queue<std::queue<int>>>>);

	//write alignments, load alignments



private:
	Model_Parms model_parms;
	Model_marginals model_marginals;
	std::pair<std::string , std::queue<std::queue<int>>> generate_unique_sequence(std::queue<Rec_Event*> , std::unordered_map<Rec_Event_name,int> , const std::unordered_map<Rec_Event_name,std::vector<std::pair<const Rec_Event*,int>>>& , std::default_random_engine& );
	Model_marginals compute_marginals(std::list<std::string> sequences);
	Model_marginals compute_seq_marginals (std::string sequence);
	Model_marginals compute_seq_marginals (std::string sequence , std::list<std::list<std::string> > allowed_scenarios );

};



#endif /* GENMODEL_H_ */