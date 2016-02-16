/*
 * GenModel.cpp
 *
 *  Created on: 3 nov. 2014
 *      Author: marcou
 *
 *      This class designs a generative model and supply all the methods to run a maximum likelihood estimate of the generative model
 */

#include "GenModel.h"

using namespace std;

GenModel::GenModel(const Model_Parms& parms): model_parms(parms) , model_marginals(*(new Model_marginals(parms))) {
	// TODO Auto-generated constructor stub

}

GenModel::GenModel(const Model_Parms& parms, const Model_marginals& marginals): model_parms(parms) , model_marginals(marginals){}

GenModel::~GenModel() {
	// TODO Auto-generated destructor stub
}

bool GenModel::infer_model(const vector<pair<string,unordered_map<Gene_class , vector<Alignment_data>>>>& sequences ,const  int iterations ,const string path , double likelihood_threshold/*=1e-25 by default*/ , double proba_threshold_factor/*=0.001 by default*/ , double mean_number_seq_err_thresh /*= INFINITY by default*/){
	queue<Rec_Event*> model_queue = model_parms.get_model_queue();
	unordered_map<Rec_Event_name,int> index_map = model_marginals.get_index_map(model_parms,model_queue);
	unordered_map<Rec_Event_name,list<pair<const Rec_Event*,int>>> inv_offset_map = model_marginals.get_inverse_offset_map(model_parms,model_queue);
	int iteration_accomplished = 0;
	ofstream log_file(path + string("inference_logs.txt"));
	log_file<<"iteration_n;seq_processed;nt_sequence;n_V_aligns;n_J_aligns;seq_likelihood;seq_mean_n_errors;seq_n_scenarios;seq_best_scenario"<<endl;
	ofstream general_logs(path + string("inference.out"));

	//Write initial condition to file
	this->model_marginals.write2txt(path+string("initial_marginals.txt"),this->model_parms);
	this->model_parms.write_model_parms(path+string("initial_model.txt"));
/*
 * Reduction using OpenMP 4.0 standards
 *
	#pragma omp declare reduction(+:Model_marginals:omp_out+=omp_in) initializer(omp_priv = omp_orig.empty_copy())
	//Note: since Error_rate is an abstract class, the following is very dirty, need to find a better solution
	#pragma omp declare reduction(+:Error_rate*:add_to_err_rate(omp_out,omp_in)) initializer(omp_priv = omp_orig->copy())
*/

	//Loop over iterations
	while(iteration_accomplished!=iterations){

		//double proba_threshold_factor;
		Model_marginals new_marginals = Model_marginals(model_parms);
		Error_rate* error_rate_copy = model_parms.get_err_rate_p()->copy();

		//Hard code Anand's probability threshold
/*		if(iteration_accomplished ==0){	proba_threshold_factor = 0.01;	}
		else if(iteration_accomplished<4){proba_threshold_factor = .005;}
		else{proba_threshold_factor = .001;}*/

		//Initialize counters for the log file
		size_t sequences_processed = 0;




		/* omp parallel declaration using OpenMP 4.0 standards
		 * #pragma omp parallel for schedule(dynamic) reduction(+:error_rate_copy,new_marginals) firstprivate(model_queue,index_map,offset_map,model_marginals_copy,events_map , processed_events , safety_set , write_index_list) //num_threads(6)
		 */

		//Declare variables to use OpenMP 3.1 standards
		#pragma omp parallel shared(new_marginals,error_rate_copy,sequences_processed) firstprivate(model_queue,proba_threshold_factor ) //num_threads(1)
		{
			//Make single thread copies of objects for thread safety
			Model_Parms single_thread_model_parms (model_parms);
			unordered_map<Rec_Event_name,int> single_thread_index_map =model_marginals.get_index_map(model_parms,model_queue);
			Model_marginals single_thread_model_marginals (model_marginals);
			Model_marginals single_thread_marginals (single_thread_model_parms);
			Error_rate* single_thread_err_rate = single_thread_model_parms.get_err_rate_p();
			unordered_map<tuple<Event_type,Gene_class,Seq_side>, Rec_Event*> events_map = single_thread_model_parms.get_events_map();


			unordered_set<Rec_Event_name> init_processed_events;

			//Initialize Enum_fast_memory map and dual maps
			Safety_bool_map safety_set(3);
			Seq_type_str_p_map constructed_sequences(6);//6 is the number of outcomes for Seq_type
			Mismatch_vectors_map mismatches_lists(6);
			Seq_offsets_map seq_offsets(6,3);

			list<Rec_Event*> events_list = single_thread_model_parms.get_event_list();
			Index_map index_mapp(events_list.size());

			//Initialize index_map
			for(list<Rec_Event*>::const_iterator event_iter = events_list.begin() ; event_iter != events_list.end() ; ++event_iter){
				int event_index = (*event_iter)->get_event_identifier();
				index_mapp.request_memory_layer(event_index);
				index_mapp.set_value(event_index,single_thread_index_map.at((*event_iter)->get_name()) , 0);
				//TODO update proba bound

				//Get events probability upper bounds
				size_t event_size = single_thread_model_marginals.get_event_size((*event_iter) , single_thread_model_parms);
				(*event_iter)->set_upper_bound_proba(single_thread_index_map.at((*event_iter)->get_name()) , event_size , single_thread_model_marginals.marginal_array_p);
			}

			queue<Rec_Event*> single_thread_model_queue = single_thread_model_parms.get_model_queue(); //single_thread_parms.get_model_queue();

			queue<Rec_Event*> init_single_thread_model_queue = single_thread_model_queue;
			unordered_map<Rec_Event_name,vector<pair<const Rec_Event*,int>>>single_thread_offset_map = model_marginals.get_offsets_map(model_parms,single_thread_model_queue);

			stack<Rec_Event*> init_single_thread_stack;

			//Initialize events
			while(!init_single_thread_model_queue.empty()){
				Rec_Event* first_init_event = init_single_thread_model_queue.front();
				init_single_thread_stack.push(first_init_event);
				init_single_thread_model_queue.pop();
				(*first_init_event).initialize_event(init_processed_events,events_map , single_thread_offset_map , constructed_sequences,safety_set , single_thread_err_rate , mismatches_lists,seq_offsets , index_mapp);
			}

			//Compute upper proba bounds for downstream scenarios for each event
			double downstream_proba_bound = 1 ;
			forward_list<double*> updated_proba_list ;
			while(!init_single_thread_stack.empty()){
				Rec_Event* last_proba_init_event = init_single_thread_stack.top();
				init_single_thread_stack.pop();
				(*last_proba_init_event).initialize_scenario_proba_bound(downstream_proba_bound , updated_proba_list , events_map);
			}




			//Loop over sequences in parallel, using the number of threads declared previously when declaring the parallel section
			//Use dynamic scheduling to avoid loss of time due to synchronization
			#pragma omp for schedule(guided) nowait
			for(vector<pair<string,unordered_map<Gene_class , vector<Alignment_data>>>>::const_iterator seq_it = sequences.begin() ; seq_it < sequences.end() ; ++seq_it){

				//Make a copy of the queue that can be modified in iterate
				queue<Rec_Event*> model_queue_copy(single_thread_model_queue);

				//Get the first event from the queue
				Rec_Event* first_event = model_queue_copy.front();
				model_queue_copy.pop();


				//Initialize single seq marginals
				Model_marginals single_seq_marginals = single_thread_model_marginals.empty_copy();
				double init_proba = 1;
				double init_tmp_err_w_proba = 1;
				double max_proba_scenario = likelihood_threshold/proba_threshold_factor;
				string int_sequence = nt2int(seq_it->first);
				//cout<<int_sequence<<endl;


				/*
				 * Call iterate on the first event
				 * The method will be called recursively for each event, this is equivalent to a nested loop and enumerates all possible scenarios
				 * The weight of each recombination scenario is added to the single_seq_marginals on the fly
				 */
				try{
					first_event->iterate(init_proba , init_tmp_err_w_proba , (*seq_it).first , int_sequence , index_mapp , single_thread_offset_map , model_queue_copy , single_seq_marginals.marginal_array_p , single_thread_model_marginals.marginal_array_p , (*seq_it).second , constructed_sequences , seq_offsets , single_thread_err_rate  , events_map , safety_set , mismatches_lists , max_proba_scenario , proba_threshold_factor);
				}
				catch(exception& except){
					general_logs<<"Exception caught calling iterate() on sequence:"<<endl;
					general_logs<<(*seq_it).first<<endl;
					general_logs<<"Exception caught after "<<single_thread_err_rate->debug_number_scenarios<<" scenarios explored"<<endl;
					general_logs<<endl;
					general_logs<<"Throwing exception now..."<<endl<<endl;
					general_logs<<except.what()<<endl;
					throw;
				}



				//Normalize the weights on the single_seq_marginal so that each sequence has the same weight when merged to the single_thread_marginals
				single_thread_err_rate->norm_weights_by_seq_likelihood(single_seq_marginals.marginal_array_p,single_seq_marginals.get_length());
				#pragma omp critical(output_0_ins_prob)
				{
					++sequences_processed;
					//Output useful infos in the log file
					//log_file<<iteration_accomplished<<";"<<sequences_processed<<";"<<(*seq_it).first<<";"<<(*seq_it).second.at(V_gene).size()<<";"<<(*seq_it).second.at(D_gene).size()<<";"<<(*seq_it).second.at(J_gene).size()<<";"<<single_thread_err_rate->get_seq_probability()<<";"<<single_thread_err_rate->get_seq_likelihood()<<";"<<single_thread_err_rate->debug_number_scenarios<<";"<<max_proba_scenario<<endl;
					log_file<<iteration_accomplished<<";"<<sequences_processed<<";"<<(*seq_it).first<<";"<<(*seq_it).second.at(V_gene).size()<<";"<<(*seq_it).second.at(J_gene).size()<<";"<<single_thread_err_rate->get_seq_likelihood()<<";"<<single_thread_err_rate->get_seq_mean_error_number()<<";"<<single_thread_err_rate->debug_number_scenarios<<";"<<max_proba_scenario<<endl;
				}

				if(single_thread_err_rate->get_seq_mean_error_number()<=mean_number_seq_err_thresh){
					//Add weigthed errors to the normalized error counter
					single_thread_err_rate->add_to_norm_counter();

					//Add the single_seq_marginals to the single thread marginals
					single_thread_marginals+=single_seq_marginals;
				}
				else{
					//Erase seq specific counters so that it won't contribute to the error rate
					single_thread_err_rate->clean_seq_counters();
				}

			}

			//Merge single thread error_rates and marginals
			#pragma omp critical(merge_marginals_and_er)
			{
				new_marginals+=single_thread_marginals;
				add_to_err_rate(error_rate_copy,single_thread_err_rate);
			}


		}
		general_logs<<"model_mean_seq_likelihood: "<<error_rate_copy->get_model_likelihood()/error_rate_copy->get_number_non_zero_likelihood_seqs()<<"; # of sequences with non zero likelihood: "<<error_rate_copy->get_number_non_zero_likelihood_seqs()<<endl;
		error_rate_copy->update();
		this->model_parms.set_error_ratep(error_rate_copy);
		new_marginals.normalize(inv_offset_map , index_map , model_queue);
		new_marginals.copy_fixed_events_marginals(this->model_marginals,this->model_parms,index_map);
		this->model_marginals = new_marginals;
		++iteration_accomplished;
		this->model_marginals.write2txt(path+string("iteration_")+to_string(iteration_accomplished)+string(".txt"),this->model_parms);
		this->model_parms.write_model_parms(path+string("iteration_")+to_string(iteration_accomplished)+string("_parms.txt"));
	}
	return 0;
}

forward_list<pair<string,queue<queue<int>>>> GenModel::generate_sequences(int number_seq , bool generate_errors){

	queue<Rec_Event*> model_queue = this->model_parms.get_model_queue();
	unordered_map<Rec_Event_name,int> index_map = this->model_marginals.get_index_map(this->model_parms,model_queue);
	unordered_map<Rec_Event_name,vector<pair<const Rec_Event* , int>>> offset_map = this->model_marginals.get_offsets_map(this->model_parms,model_queue);

	//Create seed for random generator
	//create a seed from timer
	typedef std::chrono::high_resolution_clock myclock;
	myclock::time_point time = myclock::now();
	myclock::duration dur = myclock::time_point::max() - time;

	unsigned time_seed = dur.count();
	//Instantiate random number generator
	default_random_engine generator =  default_random_engine(time_seed);
	forward_list<pair<string,queue<queue<int>>>> sequence_list = *(new forward_list<pair<string,queue<queue<int>>>>());

	for(int seq = 0 ; seq != number_seq ; ++seq){
		pair<string,queue<queue<int>>> sequence = this->generate_unique_sequence(model_queue , index_map ,offset_map , generator);
		if(generate_errors){
			sequence.second.push(this->model_parms.get_err_rate_p()->generate_errors(sequence.first,generator));
		}
		sequence_list.push_front(sequence);
	}

	return sequence_list;

}
/*
 * Generate sequences in a memory efficient way
 */
void GenModel::generate_sequences(int number_seq,bool generate_errors , string filename_ind_seq , string filename_ind_real){
	ofstream outfile_ind_seq(filename_ind_seq);
	ofstream outfile_ind_real(filename_ind_real);

	//Create a header for the files
	outfile_ind_seq<<"seq_index;nt_sequence"<<endl;
	queue<Rec_Event*> model_queue = this->model_parms.get_model_queue();
	outfile_ind_real<<"Index";
	while(!model_queue.empty()){
		outfile_ind_real<<";"<<model_queue.front()->get_name();
		model_queue.pop();
	}
	outfile_ind_real<<endl;
	model_queue = this->model_parms.get_model_queue();
	unordered_map<Rec_Event_name,int> index_map = this->model_marginals.get_index_map(this->model_parms,model_queue);
	unordered_map<Rec_Event_name,vector<pair<const Rec_Event* , int>>> offset_map = this->model_marginals.get_offsets_map(this->model_parms,model_queue);

	//Create seed for random generator
	//create a seed from timer
	typedef std::chrono::high_resolution_clock myclock;
	myclock::time_point time = myclock::now();
	myclock::duration dur = myclock::time_point::max() - time;
	//cout<<dur<<endl;
	unsigned time_seed = dur.count();
	cout<<"Seed: "<<time_seed<<endl;
	//Instantiate random number generator
	default_random_engine generator =  default_random_engine(time_seed);

	for(int seq = 0 ; seq != number_seq ; ++seq){
		pair<string,queue<queue<int>>> sequence = this->generate_unique_sequence(model_queue , index_map ,offset_map , generator);
		if(generate_errors){
			sequence.second.push(this->model_parms.get_err_rate_p()->generate_errors(sequence.first,generator));
		}
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//TODO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//string tmp = sequence.first.substr(sequence.first.size()-101,100);
		//string tmp = sequence.first.substr(sequence.first.size()-128,127);
		//sequence.first = tmp;
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


		outfile_ind_seq<<seq<<";"<<sequence.first<<endl;
		outfile_ind_real<<seq;
		queue<queue<int>> realizations = sequence.second;
		while(!realizations.empty()){
			outfile_ind_real<<";";
			queue<int> event_real = realizations.front();
			outfile_ind_real<<"(";
			while(!event_real.empty()){
				outfile_ind_real<<event_real.front();
				event_real.pop();
				if(!event_real.empty()){
					outfile_ind_real<<",";
				}
			}
			outfile_ind_real<<")";
			realizations.pop();
		}
		outfile_ind_real<<endl;

	}





}

pair<string,queue<queue<int>>> GenModel::generate_unique_sequence(queue<Rec_Event*> model_queue , unordered_map<Rec_Event_name,int> index_map , const unordered_map<Rec_Event_name,vector<pair<const Rec_Event* , int>>>& offset_map , default_random_engine& generator ){
	unordered_map<Seq_type,string>* constructed_sequences_p = new unordered_map<Seq_type,string>;
	unordered_map<Seq_type,string> constructed_sequences = *constructed_sequences_p;
	queue<queue<int>> realizations ;
	while(! model_queue.empty()){
		realizations.push(model_queue.front()->draw_random_realization((this->model_marginals.marginal_array_p) , index_map , offset_map , constructed_sequences ,  generator));
		model_queue.pop();
	}
	//CAT strings
	string reconstructed_seq = constructed_sequences[V_gene_seq] + constructed_sequences[VJ_ins_seq] + constructed_sequences[VD_ins_seq] + constructed_sequences[D_gene_seq] + constructed_sequences[DJ_ins_seq] + constructed_sequences[J_gene_seq];
	delete constructed_sequences_p;
	return make_pair(reconstructed_seq,realizations);
}

void GenModel::write_seq2txt(string filename , forward_list<string> sequences){
	ofstream outfile (filename);
	for(forward_list<string>::const_iterator seq = sequences.begin() ; seq != sequences.end() ; ++seq){
		outfile<<(*seq)<<endl;
	}
}

void GenModel::write_seq_real2txt(string filename_ind_seq , string filename_ind_real , forward_list<pair<string,queue<queue<int>>>> seq_and_realizations){
	ofstream outfile_ind_seq(filename_ind_seq);
	ofstream outfile_ind_real(filename_ind_real);

	//Create a header for the files
	outfile_ind_seq<<"seq_index;nt_sequence"<<endl;
	queue<Rec_Event*> model_queue = this->model_parms.get_model_queue();
	outfile_ind_real<<"Index";
	while(!model_queue.empty()){
		outfile_ind_real<<";"<<model_queue.front()->get_name();
		model_queue.pop();
	}
	outfile_ind_real<<endl;

	size_t index = 0;
	for(forward_list<pair<string,queue<queue<int>>>>::const_iterator iter = seq_and_realizations.begin() ; iter != seq_and_realizations.end() ; iter++){
		outfile_ind_seq<<index<<";"<<(*iter).first<<endl;
		outfile_ind_real<<index;
		queue<queue<int>> realizations = (*iter).second;
		while(!realizations.empty()){
			outfile_ind_real<<";";
			queue<int> event_real = realizations.front();
			outfile_ind_real<<"(";
			while(!event_real.empty()){
				outfile_ind_real<<event_real.front();
				event_real.pop();
				if(!event_real.empty()){
					outfile_ind_real<<",";
				}
			}
			outfile_ind_real<<")";
			realizations.pop();
		}
		outfile_ind_real<<endl;
		index++;
	}

}