/**
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <math.h>  // log
#include <float.h>
// TODO move to .h

#include "working_biSBM.h"

#define WRONG_OPTION_COUNT 1
#define UNKNOWN_GRAPH_TYPE 2
#define BAD_FILE 3
#define GRAPH_READ_FAILED 4
#define ILLEGAL_FORMAT 5


/**
 * TODO
 *     * get rid of excessive allocation
 *     * store individual variables instead of whole arrays
 *     * check for null on malloc
 *     * free all allocated memory
 *     * check for error code on all igraph calls
 */


/**
 * Function: igraph_read_graph_generic
 * -----------------------------------
 * Takes a pointer to an empty graph along with the filetype
 * and path to file, and attempts to read the file in `file_name`
 * into the ifgraph_t graph `graph` using the `type` specified.
 */
void igraph_read_graph_generic(igraph_t *graph, char *type, char *file_name){
  FILE * infile;
  infile = fopen (file_name, "r");
  if (infile == NULL){
    printf("Could not open file: %s\n", file_name);
    print_usage_and_exit(BAD_FILE);
  }

  if (!strcmp(type, "gml")){
    printf("Attempting to read in .gml file...\n");

    // TODO: this function segfaults when it fails. Submit a bugfix!
    // As such, err isn't doing anything.
    int err = igraph_read_graph_gml(graph, infile);
    fclose(infile);
    if (err) exit(GRAPH_READ_FAILED);
    return;
  }

  if (!strcmp(type, "graphml")){
    printf("Attempting to read in graphml file...\n");

    // TODO: this function aborts when it fails, so err isn't doing
    // anything.
    int err = igraph_read_graph_graphml(graph, infile, 0);
    fclose(infile);
    if (err) exit(GRAPH_READ_FAILED);
    return;
  }
  // TODO: add more types!
  print_usage_and_exit(UNKNOWN_GRAPH_TYPE);
  
}

/**
 * Function: print_usage_and_exit
 * ------------------------------
 * Prints the command line usage of the program,
 * and exits with exit status `exitstatus`.
 */
void print_usage_and_exit(int exitstatus){
  printf("\nUsage:\n");
  printf("  ./working_biSBM [graph type] [path to graph] [K_a] [K_b] [max iterations]\n");
  printf("  Where graph type is one of:\n");
  printf("    gml\n");
  printf("    graphml\n");
  // //TODO: ADD SUPPORT FOR MORE
  printf("  K_a is the number of groups of type a\n");
  printf("  K_b is the number of groups of type b\n");
  printf("  max iterations is the maximum number of iterations that the algorithm will attempt.\n\n");
  exit(exitstatus);
}


/**
 * Function: initialize_groups
 * ---------------------------
 * Given group sizes `a` and `b`, randomly initializes each vertex to 
 * a group. If the vertex type is 0, it will be randomly placed into
 * a group in the range [0, a). If it is 1, it will be randomly
 * placed into a group in the range [a, a+b)
 */
int *initialize_groups(int a, int b, igraph_vector_bool_t *types){
  int *groupings = malloc(sizeof(int) * igraph_vector_bool_size(types));
  igraph_rng_t *rng = igraph_rng_default();
  // assign seed to some constant for repeatable results.
  int seed = time(NULL); 
  igraph_rng_seed(rng, seed);
  for (int i = 0; i < igraph_vector_bool_size(types); i++){
    if (!VECTOR(*types)[i]) // type 0
      groupings[i] = igraph_rng_get_integer(rng, 0, a - 1);
    else // type 1
      groupings[i] = igraph_rng_get_integer(rng, a, a + b - 1);
  }
  return groupings;
}



double calculate_term(int *partition, igraph_t *graph, int r, int s, igraph_eit_t *eit){
  int m_rs = 0;
  int k_r = 0;
  int k_s = 0;
  igraph_real_t r_type[igraph_vcount(graph)];
  int r_len = 0;
  igraph_real_t s_type[igraph_vcount(graph)];
  int s_len = 0;

  for (int v = 0; v < igraph_vcount(graph); v++){
    if (partition[v] == r){
      r_type[r_len] = v;
      r_len++;
    }
    else if (partition[v] == s){
      s_type[s_len] = v;
      s_len++;
    }
  }

  igraph_bool_t res;

  for (int vr = 0; vr < r_len; vr++){
    for (int vs = 0; vs < s_len; vs++){
      // do something else, this is really slow.
      igraph_are_connected(graph, r_type[vr], s_type[vs], &res);
      if (res)
        m_rs++;
    }
  }

  const igraph_vector_t r_view;
  igraph_vector_view(&r_view, r_type, r_len);

  const igraph_vector_t s_view;
  igraph_vector_view(&s_view, s_type, s_len);

  igraph_vector_t r_deg;
  igraph_vector_t s_deg;
  igraph_vector_init(&r_deg, r_len);
  igraph_vector_init(&s_deg, s_len);

  // assumes there are no loops, so it doesn't hurt to count them.
  // also assumes undirected.
  igraph_degree(graph, &r_deg, igraph_vss_vector(&r_view), IGRAPH_OUT, true);
  igraph_degree(graph, &s_deg, igraph_vss_vector(&s_view), IGRAPH_OUT, true);

  for (int i = 0; i < r_len; i++)
    k_r += VECTOR(r_deg)[i];
  for (int i = 0; i < s_len; i++)
    k_s += VECTOR(s_deg)[i];
  //printf("m_rs: %d, %d, %d\n", m_rs, k_r, k_s);
  igraph_vector_destroy(&r_deg);
  igraph_vector_destroy(&s_deg);
  // // check
  if (k_r * k_s == 0) return -DBL_MAX;
  if (m_rs == 0) return -DBL_MAX;
 
  double term_2 = log((double) m_rs / (double) (k_r * k_s));
  //printf("term2: %f\n", term_2);
  return m_rs * term_2;

  // check to see if igraph_edge would be faster using the iterator below
  // IGRAPH_EIT_RESET(eit);
  // for (int e = 0; e < IGRAPH_EIT_SIZE(eit); e++){
  //   IGRAPH_EIT_NEXT(eit);
  //   IGRAPH_EIT_GET(eit);
  // }
  // TODO: check if igraph_es_fromto is implemented yet.

}


/**
 * Function: score_partition
 * -------------------------
 * TODO
 */
 // TODO: should be able to score whole thing once then make minor modifications
 // with individual switches.
double score_partition_degree_corrected(int *partition, igraph_t *graph, int a, int b){
  //printf("scoring...\n");
  double log_likelihood = 0;
  igraph_es_t es;
  igraph_es_all(&es, IGRAPH_EDGEORDER_FROM);
  igraph_eit_t eit;
  igraph_eit_create(graph, es, &eit);
  for (int r = 0; r < a; r++){
    for (int s = a; s < a + b; s++){
      log_likelihood += calculate_term(partition, graph, r, s, &eit);
    }
  }
  igraph_eit_destroy(&eit);
  return log_likelihood;
}


/**
 * Function: swap_and_score
 * ------------------------
 * Scores the partition `partition` where partition[v] = group. 
 * If the group assignment is illegal, returns -1. 
 */
double swap_and_score(int *partition, int v, int a, int b, int group, igraph_t *graph){
  // the swap wouldn't do anything or it would swap to the wrong type
  if ((partition[v] == group) || (partition[v] < a && group >= a) || (partition[v] >= a && group < a)){
    return -DBL_MAX;
  }
  int temp = partition[v];
  partition[v] = group;
  double score = score_partition_degree_corrected(partition, graph, a, b);
  partition[v] = temp;
  return score;
}


/**
 * Function: make_best_switch
 * --------------------------
 * Tries all possible switches between all vertices and groups of the same type.
 * Modifies `partition` to reflect the best switch and `used` to store the vertex switched.
 * Returns the score of the switch made.
 */
double make_best_switch(int *partition, int a, int b, igraph_vector_bool_t *types, igraph_t *graph, bool *used){
  int best_switch_vertex = -1;
  int best_switch_group = -1;
  double best_switch_score = -DBL_MAX; 
  for (int v = 0; v < igraph_vector_bool_size(types); v++){
    if (used[v]){ // can't swap because we've already swapped this vertex.
      continue; // meh. don't do it this way. TODO
    }
    double best_curr_score = -DBL_MAX;
    int best_curr_group = -1;
    for (int group = 0; group < a + b; group++){
      // Try swapping with all other groups
      double score = swap_and_score(partition, v, a, b, group, graph);
      if (score > best_curr_score){
        best_curr_score = score;
        best_curr_group = group;
      }
    }
    if (best_curr_score > best_switch_score){
      best_switch_vertex = v;
      best_switch_group = best_curr_group;
      best_switch_score = best_curr_score;
    }
  }
  // modify partition to reflect the best swap.
  partition[best_switch_vertex] = best_switch_group;
  used[best_switch_vertex] = true;
  return best_switch_score;
}


/**
 * Function: iterate_once
 * ----------------------
 * Given some initial state described in `partition`, moves all vertices once to attempt to maximize
 * the MLE described in the paper. Returns the maximum score, and stores the grouping that yields
 * that score in `partition`.
 */
double iterate_once(int *partition, int a, int b, igraph_vector_bool_t *types, igraph_t *graph){
  //int *working_partition = malloc(sizeof(int) * igraph_vector_bool_size(types)); // TODO: allocate on stack
  int working_partition[igraph_vector_bool_size(types)];
  memcpy(working_partition, partition, sizeof(int) * igraph_vector_bool_size(types));

  double max_score = -DBL_MAX;
  int num_used = 0;

  // initialize to 0
  bool used[igraph_vector_bool_size(types)];
  for (int i = 0; i < igraph_vector_bool_size(types); i++) used[i] = false;
  // bool *used = calloc(igraph_vector_bool_size(types), sizeof(bool)); // TODO: allocate on stack

  while (num_used < igraph_vector_bool_size(types)){
    double new_score = make_best_switch(working_partition, a, b, types, graph, used);
    if (new_score > max_score){
      max_score = new_score;
      memcpy(partition, working_partition, sizeof(int) * igraph_vector_bool_size(types));
    }
    num_used++;
    printf("Used: %d\n", num_used);
  }
  return max_score;
}


/**
 * Function: run_algorithm
 * -----------------------
 * Attempts to run the biSBM algorithm from the initialization state `partition` for a maximum of
 * max_iters iterations. See the paper for more information.
 * http://danlarremore.com/pdf/2014_LCJ_EfficientlyInferringCommunityStructureInBipartiteNetworks_PRE.pdf
 */
double run_algorithm(int *partition, int a, int b, igraph_vector_bool_t *types, igraph_t *graph, int max_iters){
  int latest_grouping[igraph_vector_bool_size(types)];
  //int *latest_grouping = malloc(sizeof(int) * igraph_vector_bool_size(types));

  // working partition (latest_grouping) gets partition
  memcpy(latest_grouping, partition, sizeof(int) * igraph_vector_bool_size(types));
  double score = score_partition_degree_corrected(partition, graph, a, b);
  printf("Initial score: %f\n", score);
  for (int i = 0; i < max_iters; i++){
    printf("Performing iteration %d...\n", i);
    double new_score = iterate_once(latest_grouping, a, b, types, graph);
    if (new_score <= score) {
      printf("Score has not improved. Algorithm has converged.\n");
      // partition is not changed
      return score;
    }
    score = new_score;
    // partition is improved to latest_grouping!
    memcpy(partition, latest_grouping, sizeof(int) * igraph_vector_bool_size(types));
  }
  return score;
}


int main(int argc, char *argv[]) {
  if (argc != 6) // TODO: check all arguments
    print_usage_and_exit(WRONG_OPTION_COUNT);
  igraph_t graph;
  igraph_read_graph_generic(&graph, argv[1], argv[2]);
  int a = atoi(argv[3]);
  int b = atoi(argv[4]);
  int max_iters = atoi(argv[5]);


  printf("Graph with %d vertices and %d edges read successfully.\n"
            , igraph_vcount(&graph), igraph_ecount(&graph)); 

  igraph_bool_t is_bipartite;
  igraph_vector_bool_t types;
  igraph_vector_bool_init(&types, igraph_vcount(&graph));

  printf("Finding a bipartite mapping...\n");
  // TODO: check for error code
  igraph_is_bipartite(&graph, &is_bipartite, &types);
  if (!is_bipartite){
    printf("Input graph is not bipartite. Exiting...\n");
    exit(ILLEGAL_FORMAT);
  }
  printf("Mapping successful.\n");

  printf("Initializing to random groups...\n");
  int *partition = initialize_groups(a, b, &types);
  printf("Initialization successful.\n");

  printf("Running algorithm...\n");
  double best_score = run_algorithm(partition, a, b, &types, &graph, max_iters);
  printf("Algorithm completed successfully.\n");

  printf("%f\n", best_score);
  for (int i = 0; i < igraph_vcount(&graph); i++){
    printf("Vertex %d: group %d\n", i, partition[i]);
  }
  free(partition);
  igraph_vector_bool_destroy(&types);
  igraph_destroy(&graph);
  return 0;

}
