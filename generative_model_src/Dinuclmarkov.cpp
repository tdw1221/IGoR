/*
 * Dinuclmarkov.cpp
 *
 *  Created on: Mar 4, 2015
 *      Author: quentin
 */

#include "Dinuclmarkov.h"

using namespace std;

Dinucl_markov::Dinucl_markov(Gene_class gene): Rec_Event()  ,  total_nucl_count(0) {
	this->type = Event_type::Dinuclmarkov_t;
	event_class = gene;
	//Same indexes as Aligner::nt2int
	event_realizations.emplace("A" ,*(new Event_realization("A",INT16_MAX,"A","",0)));
	event_realizations.emplace("C" ,*(new Event_realization("C",INT16_MAX,"C","",1)));
	event_realizations.emplace("G" ,*(new Event_realization("G",INT16_MAX,"G","",2)));
	event_realizations.emplace("T" ,*(new Event_realization("T",INT16_MAX,"T","",3)));
	updated = true;
	updated_upper_bound_proba = new double;
	this->update_event_name();

}



Dinucl_markov::~Dinucl_markov() {
	// TODO Auto-generated destructor stub
	delete updated_upper_bound_proba;
}

Rec_Event* Dinucl_markov::copy(){
	//TODO rewrite this by invoking a copy constructor?
	Dinucl_markov* new_dinucl_markov_p = new Dinucl_markov(this->event_class);
	new_dinucl_markov_p->priority = this->priority;
	new_dinucl_markov_p->nickname = this->nickname;
	new_dinucl_markov_p->fixed = this->fixed;
	new_dinucl_markov_p->update_event_name();
	new_dinucl_markov_p->set_event_identifier(this->event_index);
	return new_dinucl_markov_p;
}

int Dinucl_markov::size()const{
	return event_realizations.size()*event_realizations.size();
}


void Dinucl_markov::iterate(double& scenario_proba , double& tmp_err_w_proba , const string& sequence , const string& int_sequence , Index_map& base_index_map , const unordered_map<Rec_Event_name,vector<pair<const Rec_Event*,int>>>& offset_map , queue<Rec_Event*>& model_queue , Marginal_array_p& updated_marginals_point , const Marginal_array_p& model_parameters_point ,const unordered_map<Gene_class , vector<Alignment_data>>& allowed_realizations , Seq_type_str_p_map& constructed_sequences , Seq_offsets_map& seq_offsets ,Error_rate*& error_rate_p , const unordered_map<tuple<Event_type,Gene_class,Seq_side>, Rec_Event*>& events_map , Safety_bool_map& safety_set , Mismatch_vectors_map& mismatches_lists, double& seq_max_prob_scenario , double& proba_threshold_factor){
	base_index = base_index_map.at(this->event_index);
	new_scenario_proba = scenario_proba;
	proba_contribution = 1;


	correct_class=0;
	//For now do not include possible sequencing error
	if(event_class == VD_genes || event_class == VDJ_genes){
		correct_class = 1;
		previous_seq = (*constructed_sequences.at(V_gene_seq));
		vd_seq = (*constructed_sequences.at(VD_ins_seq));
		vd_seq_size = vd_seq.size();
		previous_seq_size = previous_seq.size();
		//data_seq_substr = sequence.substr(seq_offsets.at(pair<Seq_type,Seq_side>(V_gene_seq,Five_prime)) + previous_seq_size , vd_seq.size());
		//data_seq_substr = sequence.substr(seq_offsets.at(v_5_pair) + previous_seq_size , vd_seq.size());//TODO check this
		//data_seq_substr = int_sequence.substr(seq_offsets.at(v_5_pair) + previous_seq_size , vd_seq.size());
		data_seq_substr = int_sequence.substr(seq_offsets.at(V_gene_seq,Five_prime) + previous_seq_size , vd_seq.size());

		previous_nt_str = previous_seq.substr(previous_seq_size-1,1);
		iterate_common( vd_realizations_indices , previous_nt_str  , vd_seq , model_parameters_point);
		//constructed_sequences.at(VD_ins_seq) = &vd_seq;

	}
	if(event_class == DJ_genes || event_class == VDJ_genes){
		correct_class = 1;
/*		//TODO check the side taken into account for the first nucleotide
		string d_seq = constructed_sequences.at(D_gene_seq);
		size_t d_seq_size = d_seq.size();

		size_t char_index = seq_offsets.at(pair<Seq_type,Seq_side>(V_gene_seq,Five_prime)) + constructed_sequences.at(V_gene_seq).size() + constructed_sequences.at(VD_ins_seq).size() + d_seq_size ;//TODO rewrite this and once deletion mechanism has been rewritten
		string data_seq_substr = sequence.substr(char_index , constructed_sequences.at(DJ_ins_seq).size());
		new_scenario_proba = iterate_common(new_scenario_proba , prev_seq.substr(prev_seq_size-1,1) , data_seq_substr , constructed_sequences.at(DJ_ins_seq) , base_index , write_index_list , model_parameters_point);
		*/
		previous_seq = (*constructed_sequences.at(J_gene_seq));
		dj_seq = (*constructed_sequences.at(DJ_ins_seq));
		dj_seq_size = dj_seq.size();
		//string& constr_ins_seq = constructed_sequences.at(DJ_ins_seq);

		//size_t char_index = seq_offsets.at(pair<Seq_type,Seq_side>(J_gene_seq,Five_prime)) -dj_seq.size();
		//size_t char_index = seq_offsets.at(j_5_pair) -dj_seq.size();
		size_t char_index = seq_offsets.at(J_gene_seq,Five_prime) -dj_seq.size();

		//data_seq_substr = sequence.substr(char_index , dj_seq.size());
		data_seq_substr = int_sequence.substr(char_index , dj_seq.size());


		previous_nt_str = previous_seq.substr(0,1);
		reverse(data_seq_substr.begin(),data_seq_substr.end());
		iterate_common( dj_realizations_indices , previous_nt_str , dj_seq , model_parameters_point);
		reverse(dj_seq.begin(),dj_seq.end());
	}
	if(event_class == VJ_genes){
		correct_class = 1;
		previous_seq = (*constructed_sequences.at(V_gene_seq));
		vj_seq = (*constructed_sequences.at(VJ_ins_seq));
		vj_seq_size = vj_seq.size();
		previous_seq_size = previous_seq.size();
		//data_seq_substr = sequence.substr(seq_offsets.at(v_5_pair) + previous_seq_size , vj_seq.size());
		//data_seq_substr = int_sequence.substr(seq_offsets.at(v_5_pair) + previous_seq_size , vj_seq.size());
		data_seq_substr = int_sequence.substr(seq_offsets.at(V_gene_seq,Five_prime) + previous_seq_size , vj_seq.size());


		previous_nt_str = previous_seq.substr(previous_seq_size-1,1);
		iterate_common( vj_realizations_indices , previous_nt_str , vj_seq , model_parameters_point);
	}
	if(!correct_class){
		throw invalid_argument("Unknown gene class for DincuclMarkov model: " + this->event_class);
	}
	tmp_err_w_proba*=proba_contribution;
	new_scenario_proba*=proba_contribution;
	compute_upper_bound_scenario_proba(tmp_err_w_proba);
	if(scenario_upper_bound_proba>=(seq_max_prob_scenario*proba_threshold_factor)){

	}
	//FIXME check this
	iterate_wrap_up(new_scenario_proba , tmp_err_w_proba , sequence , int_sequence , base_index_map , offset_map , model_queue  , updated_marginals_point  , model_parameters_point , allowed_realizations , constructed_sequences , seq_offsets , error_rate_p , events_map , safety_set , mismatches_lists ,seq_max_prob_scenario , proba_threshold_factor);

}

queue<int> Dinucl_markov::draw_random_realization(const Marginal_array_p model_marginals_p , unordered_map<Rec_Event_name,int>& index_map , const unordered_map<Rec_Event_name,vector<pair<const Rec_Event*,int>>>& offset_map , unordered_map<Seq_type , string>& constructed_sequences , default_random_engine& generator)const{

	uniform_real_distribution<double> distribution(0.0,1.0);
	bool correct_class=0;
	int index = index_map.at(this->get_name());
	queue<int> realization_queue;

	if(event_class == VD_genes || event_class == VDJ_genes){
		correct_class=1;
		string& vd_ins_seq = constructed_sequences.at(VD_ins_seq);
		string v_seq = constructed_sequences.at(V_gene_seq);

		queue<int> tmp = this->draw_random_common(v_seq , vd_ins_seq , model_marginals_p , index , distribution , generator);
		while(!tmp.empty()){
			realization_queue.push(tmp.front());
			tmp.pop();
		}


	}
	if(event_class == DJ_genes || event_class == VDJ_genes){
		correct_class=1;

		string& dj_ins_seq = constructed_sequences.at(DJ_ins_seq);
		string j_seq = constructed_sequences.at(J_gene_seq);
		reverse(j_seq.begin(),j_seq.end());

		queue<int> tmp = this->draw_random_common(j_seq , dj_ins_seq , model_marginals_p , index , distribution , generator);
		while(!tmp.empty()){
			realization_queue.push(tmp.front());
			tmp.pop();
		}
		reverse(dj_ins_seq.begin(),dj_ins_seq.end());

	}
	if(event_class == VJ_genes){
		correct_class=1;

		string& vj_ins_seq = constructed_sequences.at(VJ_ins_seq);
		string v_seq = constructed_sequences.at(V_gene_seq);

		queue<int> tmp = this->draw_random_common(v_seq , vj_ins_seq , model_marginals_p , index , distribution , generator);
		while(!tmp.empty()){
			realization_queue.push(tmp.front());
			tmp.pop();
		}

	}
	if(! correct_class){
		throw invalid_argument("Unknown gene class for DincuclMarkov model: " + this->event_class);
	}
	return realization_queue;
}

queue<int> Dinucl_markov::draw_random_common(const string& previous_seq , string& inserted_seq ,const Marginal_array_p model_marginals_p , int index , uniform_real_distribution<double>&  distribution , default_random_engine& generator)const{

	queue<int> realization_queue;
	double prob_count;
	if(!inserted_seq.empty()){
		double rand;
		if(inserted_seq[0]=='I'){
			rand = distribution(generator);
			prob_count = 0;
			int offset;
			try{
				offset = event_realizations.at(previous_seq.substr(previous_seq.size()-1,1)).index*event_realizations.size();
			}
			catch(exception& except){
				cout<<"exception caught 1, key used: "<<previous_seq.substr(previous_seq.size()-1,1);
				throw except;
			}
			for(unordered_map<string,Event_realization>::const_iterator iter = event_realizations.begin() ; iter != event_realizations.end() ; ++iter){
				prob_count += model_marginals_p[index + offset + (*iter).second.index];
				if(prob_count>=rand){
					inserted_seq[0] = (*iter).second.value_str[0];
					realization_queue.push((*iter).second.index);
					break;
				}
			}
		}
		for(size_t i=1 ; i!=inserted_seq.size() ; ++i){
			if(inserted_seq[i]=='I'){

				int offset;
				try{
					offset = event_realizations.at(inserted_seq.substr(i-1,1)).index*event_realizations.size();
				}
				catch(exception& except){
					cout<<"exception caught, key used: "<<inserted_seq.substr(i-1,1)<<",ins seq: "<<inserted_seq<<",i = "<<i<<", previous rand: "<<rand<<", previous prob_count: "<<prob_count<<endl;
					throw except;
				}
				rand = distribution(generator);
				prob_count = 0;

				for(unordered_map<string,Event_realization>::const_iterator iter = event_realizations.begin() ; iter != event_realizations.end() ; ++iter){
					prob_count += model_marginals_p[index + offset + (*iter).second.index];
					if(prob_count>=rand){
						inserted_seq[i] = (*iter).second.value_str[0];
						realization_queue.push((*iter).second.index);
						break;
					}
				}
			}
		}
	}
	return realization_queue;

}



void Dinucl_markov::write2txt(ofstream& outfile){
	outfile<<"#DinucMarkov;"<<event_class<<";"<<event_side<<";"<<priority<<";"<<nickname<<endl;
	for(unordered_map<string,Event_realization>::const_iterator iter = event_realizations.begin() ; iter != event_realizations.end() ; ++iter){
		outfile<<"%"<<(*iter).second.value_str<<";"<<(*iter).second.index<<endl;
	}
}

void Dinucl_markov::iterate_common( int* indices_array , string& previous_assigned_nt  , string& ins_seq , const Marginal_array_p model_parameters_point){

	if(!ins_seq.empty()){
		if(ins_seq.at(0)=='I'){
			//first_nt_index = event_realizations.at(previous_assigned_nt).index;
			//sec_nt_index = event_realizations.at(data_seq_substr.substr(0,1)).index;

			first_nt_index = previous_assigned_nt[0] -'0';
			sec_nt_index = data_seq_substr[0] -'0';

			//For this Dinucl_Markov model the values on the marginal array represents the conditional probability of a couple of nucleotides (N2 | N1)
			offset =  first_nt_index*event_realizations.size();
			realization_final_index = base_index + offset + sec_nt_index;
			proba_contribution*= model_parameters_point[realization_final_index];///compute_nt_freq(base_index+offset , model_parameters_point);
			indices_array[0] = realization_final_index;
			ins_seq.at(0) = data_seq_substr.at(0);
			total_nucl_count+=1;
		}

		for(size_t i = 1 ; i!= ins_seq.size() ; ++i){
			if(ins_seq.at(i)=='I'){
				//first_nt_index = event_realizations.at(data_seq_substr.substr(i-1,1)).index;
				//sec_nt_index = event_realizations.at(data_seq_substr.substr(i,1)).index;

				first_nt_index = data_seq_substr[i-1] -'0';
				sec_nt_index = data_seq_substr[i] -'0';

				//For this Dinucl_Markov model the values on the marginal array represents the joint probability of a couple of nucleotides (N1 , N2)
				offset =  first_nt_index*event_realizations.size();
				realization_final_index = base_index + offset + sec_nt_index;
				proba_contribution*= model_parameters_point[base_index + offset + sec_nt_index];///compute_nt_freq(base_index+offset , model_parameters_point);
				indices_array[i] = realization_final_index;
				ins_seq.at(i) = data_seq_substr.at(i);
				total_nucl_count+=1;
			}
		}
	}

}


/*
 * This way of proceeding is highly not optimal, should be modified
 */
double Dinucl_markov::compute_nt_freq(int index , const Marginal_array_p model_marginals) const{
	double nucl_freq = 0;
	for(size_t i = 0 ; i != event_realizations.size() ; ++i){
		nucl_freq+=model_marginals[index + i];
	}
	return nucl_freq;
}

void Dinucl_markov::ind_normalize(Marginal_array_p marginal_array_p , size_t base_index){
	size_t numb_realizations = this->event_realizations.size();
	for(size_t i = 0 ; i != numb_realizations ; ++i){
		long double sum_marginals = 0;
		for(size_t j=0 ; j != numb_realizations ; ++j){
			sum_marginals += marginal_array_p[base_index + i*numb_realizations + j];
		}
		if(sum_marginals != 0){
			for(size_t j=0 ; j != numb_realizations ; ++j){
				marginal_array_p[base_index + i*numb_realizations + j]/=sum_marginals;
			}
		}

	}
}

void Dinucl_markov::initialize_event( unordered_set<Rec_Event_name>& processed_events , const unordered_map<tuple<Event_type,Gene_class,Seq_side>, Rec_Event*>& events_map , const unordered_map<Rec_Event_name,vector<pair<const Rec_Event*,int>>>& offset_map , Seq_type_str_p_map& constructed_sequences , Safety_bool_map& safety_set , Error_rate* error_rate_p , Mismatch_vectors_map& mismatches_list,Seq_offsets_map& seq_offsets , Index_map& index_map){
	if(events_map.count(tuple<Event_type,Gene_class,Seq_side>(Insertion_t,VD_genes,Undefined_side))!=0){
		const Rec_Event* ins_vd_p = events_map.at(tuple<Event_type,Gene_class,Seq_side>(Insertion_t,VD_genes,Undefined_side));
		max_vd_ins=ins_vd_p->get_len_max();
	}
	else{
		max_vd_ins = 0;
	}
	if(events_map.count(tuple<Event_type,Gene_class,Seq_side>(Insertion_t,VJ_genes,Undefined_side))!=0){
		const Rec_Event* ins_vj_p = events_map.at(tuple<Event_type,Gene_class,Seq_side>(Insertion_t,VJ_genes,Undefined_side));
		max_vj_ins=ins_vj_p->get_len_max();
	}
	else{
		max_vj_ins=0;
	}
	if(events_map.count(tuple<Event_type,Gene_class,Seq_side>(Insertion_t,DJ_genes,Undefined_side))!=0){
		const Rec_Event* ins_dj_p = events_map.at(tuple<Event_type,Gene_class,Seq_side>(Insertion_t,DJ_genes,Undefined_side));
		max_dj_ins = ins_dj_p->get_len_max();
	}
	else{
		max_dj_ins = 0;
	}

	vd_realizations_indices = new int [max_vd_ins];
	vj_realizations_indices = new int [max_vj_ins];
	dj_realizations_indices = new int [max_dj_ins];
	this->Rec_Event::initialize_event(processed_events,events_map,offset_map,constructed_sequences,safety_set,error_rate_p,mismatches_list,seq_offsets,index_map);
}

void Dinucl_markov::add_to_marginals(long double scenario_proba , Marginal_array_p updated_marginals) const{
	if(event_class == VD_genes || event_class == VDJ_genes){
		for(size_t i = 0 ; i != vd_seq_size ; ++i){
			updated_marginals[vd_realizations_indices[i]] +=scenario_proba;
		}
	}
	if(event_class == DJ_genes || event_class == VDJ_genes){
		for(size_t i = 0 ; i != dj_seq_size ; ++i){
			updated_marginals[dj_realizations_indices[i]] +=scenario_proba;
		}
	}
	if(event_class == VJ_genes){
		for(size_t i = 0 ; i != vj_seq_size ; ++i){
			updated_marginals[vj_realizations_indices[i]] +=scenario_proba;
		}
	}
}

double* Dinucl_markov::get_updated_ptr(){
	return updated_upper_bound_proba;
}

void Dinucl_markov::initialize_scenario_proba_bound(double& downstream_proba_bound , forward_list<double*>& updated_proba_list , const unordered_map<tuple<Event_type,Gene_class,Seq_side>, Rec_Event*>& events_map){
	this->scenario_downstream_upper_bound_proba = downstream_proba_bound;
	this->updated_proba_bounds_list = updated_proba_list;
	updated_proba_list.push_front(this->updated_upper_bound_proba);
}
