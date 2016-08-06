/* lirs.c 
 *  
 * See Sigmetrics'02 paper "`LIRS: An Efficient Low Inter-reference 
 * Recency Set Replacement Policy to Improve Buffer Cache Performance"
 * for more description. "The paper" is used to refer to this paper in the 
 * following.
 *  
 * This program is written by Song Jiang (sjiang@cs.wm.edu) Nov 15, 2002
 */

/* Input File: 
 *              trace file(.trc)
 *              parameter file(.par)
 *
 * Output File: 
 *              hit rate file(.cuv): it describes the hit rates for each 
 *              cache size specified in parameter file. Gnuplot can use it 
 *              to draw hit rate curve. 
 *
 *              stack size file (.sln): It is used to produce LIRS 
 *              stack size variance figures for a specific cache size like 
 *              Fig.5 in the paper. Be noted that only the results for the
 *              last cache size are recorded in the file if multiple cache 
 *              sizes are specified in the parameter file.  
 */

/* Input File Format: 
 * (1) trace file: the (UBN) Unique Block Number of each reference, which
 *     is the unique number for each accessed block. It is strongly recommended
 *     that all blocks are mapped into 0 ... N-1 (or 1 ... N) if the total  
 *     access blocks is N. For example, if the accessed block numbers are:
 *     52312, 13456, 52312, 13456, 72345, then N = 3, and what appears in the 
 *     trace file is 0 1 0 1 2 (or 1 2 1 2 3). You can write a program using 
 *     hash table to do the trace conversion, or modify the program. 
 * (2) parameter file: 
 *      one or more cache sizes you want to test 
 *     
 */

/* Command Line Uasge: only prefix of trace file is required. e.g.
   :/ lirs ABC
   It is implied that trace file is "ABC.trc", parameter file is "ABC.par"
   output files are "ABC_LIRS.cuv" and "ABC_LIRS.sln"
*/

/* BE NOTED: If you want to place a limit on LIRS stack, or want to test
 *           hit rates for warm cache, go to lirs.h to change corresponding
 *           parameters.
 */

#include "lirs.h" 

main(int argc, char* argv[])
{
  FILE *trace_fp, *cuv_fp, *sln_fp, *para_fp;
  unsigned long i;
  int opt;
  char trc_file_name[100]; 
  char para_file_name[100];
  char cuv_file_name[100];
  char sln_file_name[100];

  if (argc != 2){
    printf("%s file_name_prefix[.trac] \n", argv[0]);
    return;
  }
  
  strcpy(para_file_name, argv[1]);
  strcat(para_file_name, ".par");
  para_fp = openReadFile(para_file_name);   //cache size

  strcpy(trc_file_name, argv[1]);
  strcat(trc_file_name, ".trc");         
  trace_fp = openReadFile(trc_file_name);  //trace

  strcpy(cuv_file_name, argv[1]);  
  strcpy(sln_file_name, argv[1]); 

  strcat(cuv_file_name, "_LIRS.cuv");
  strcat(sln_file_name, "_LIRS.sln");  
 
  cuv_fp = fopen(cuv_file_name, "w");

  if (!get_range(trace_fp, &vm_size, &total_pg_refs)){  //total_pg_refs maybe use to caculate hir ratio
    printf("trace error!\n");
    return;
  }
  
  /* Actually read the size of whole reference space and trace length */ 
  fscanf(para_fp, "%ld", &mem_size);

  page_tbl = (page_struct *)calloc(vm_size+1, sizeof(page_struct));  //page_tbl = LBN max number + 1 

  while (!feof(para_fp)){
    if (mem_size < 10){
      printf("WARNING: Too small cache size(%lu). \n", mem_size);
      break;
    }

    sln_fp = fopen(sln_file_name, "w");
    printf("\nmem_size = %lu\n", mem_size);

    total_pg_refs = 0;
    warm_pg_refs = 0;
    no_dup_refs = 0;
    num_pg_flt = 0;
    cur_lir_S_len = 0;

    fseek(trace_fp, 0, SEEK_SET);

    free_mem_size = mem_size;

    /* initialize the page table */
    for (i = 0; i <= vm_size; i++){
      page_tbl[i].ref_times = 0;           //Reference time
      page_tbl[i].pf_times = 0;            //page fault time

      page_tbl[i].page_num = i;            //page_number
      page_tbl[i].isResident = 0;          // 0 = nonresident , 1 = resident
      page_tbl[i].isHIR_block = 1;         // 1 = HIR , 0 = LIR

      page_tbl[i].LIRS_next = NULL;        //stack S
      page_tbl[i].LIRS_prev = NULL;        //stack S

      page_tbl[i].HIR_rsd_next = NULL;     //stack Q
      page_tbl[i].HIR_rsd_prev = NULL;     //stack Q

      page_tbl[i].recency = S_STACK_OUT;   //with metadata in stack S or not , out = without metadata , in = with metadata
    }

    LRU_list_head = NULL;                  //stack S
    LRU_list_tail = NULL;
    
    HIR_list_head = NULL;                  //stack Q
    HIR_list_tail = NULL;

    LIR_LRU_block_ptr = NULL;              //maximum LIR block in Stack S 

    /* the memory ratio for hirs is 1% */
    HIR_block_portion_limit = (unsigned long)(HIR_RATE/100.0*mem_size);   //stack Q size
    if (HIR_block_portion_limit < LOWEST_HG_NUM)
      HIR_block_portion_limit = LOWEST_HG_NUM;

    printf("Lhirs (cache size for HIR blocks) = %lu\n", HIR_block_portion_limit);
    LIRS_Repl(trace_fp, sln_fp);
   
    printf("total blocks refs = %lu  number of misses = %lu \nhit rate = %2.1f%%, mem shortage ratio = %2.1f%% \n"
    	,total_pg_refs,num_pg_flt,(1-(float)num_pg_flt/warm_pg_refs)*100,(float)mem_size/vm_size*100);

    fprintf(cuv_fp, "%5lu  %2.1f\n", mem_size, 100-(float)num_pg_flt/warm_pg_refs*100);
    if (sln_fp)
      fclose(sln_fp);
    fscanf(para_fp, "%lu", &mem_size);
  }

  return;
}
  


FILE *openReadFile(char file_name[])
{
  FILE *fp;

  fp = fopen(file_name, "r");

  if (!fp) {
    printf("can not find file %s.\n", file_name);
    return NULL;
  }
  
  return fp;
}


LIRS_Repl(FILE *trace_fp, FILE *sln_fp)
{
  unsigned long ref_block, i, j, step;
  long last_ref_pg = -1;
  long num_LIR_pgs = 0; 
  struct pf_struct *temp_ptr;
  int collect_stat = (STAT_START_POINT==0)?1:0;
  int count=0;

  
  fseek(trace_fp, 0, SEEK_SET);  

  fscanf(trace_fp, "%lu", &ref_block);

  i = 0;
  while (!feof(trace_fp)){
    total_pg_refs++;
    if (total_pg_refs % 10000 == 0)
      fprintf(stderr, "%lu samples processed\r", total_pg_refs);
    if (total_pg_refs > STAT_START_POINT){
      collect_stat = 1;
      warm_pg_refs++;                                          //warm?
    }
      
    if (ref_block > vm_size){
      printf("Wrong ref page number found: %lu.\n", ref_block);
      return FALSE;
    }
    
    if (ref_block == last_ref_pg){
      fscanf(trace_fp, "%lu", &ref_block);
      continue;
    }
    else
      last_ref_pg = ref_block;

    no_dup_refs++; /* ref counter excluding duplicate refs */

    if (!page_tbl[ref_block].isResident) {  /* block miss */
      if (collect_stat == 1)
	num_pg_flt++;

      if (free_mem_size == 0){                             //mean Stack S and Q full
	/* remove the "front" of the HIR resident page from cache (queue Q), 
	   but not from LIRS stack S 
	*/ 
	/* actually Q is an LRU stack, "front" is the bottom of the stack,
	   "end" is its top
	*/
	HIR_list_tail->isResident = FALSE;   //stack Q bottom
	remove_HIR_list(HIR_list_tail);      //remove stack Q bottom block
	free_mem_size++;
      }
      else if (free_mem_size > HIR_block_portion_limit){  //Llirs length enough , as LIR block
	page_tbl[ref_block].isHIR_block = FALSE;        // Stack S not full
	num_LIR_pgs++;                  //LIR block counter
      }
      free_mem_size--;
    } 
    /* hit in the cache */
    else if (page_tbl[ref_block].isHIR_block)                 
      remove_HIR_list((page_struct *)&page_tbl[ref_block]);   //remove from stack Q



    remove_LIRS_list((page_struct *)&page_tbl[ref_block]);    //remove from stack S, find maximum LIR block
    /* place newly referenced page at head */
    add_LRU_list_head((page_struct *)&page_tbl[ref_block]);   //add block to stack S top
    page_tbl[ref_block].isResident = TRUE;
    
    if (page_tbl[ref_block].recency == S_STACK_OUT)     //block not in stack S 
      cur_lir_S_len++;                                  //stack S length, HIR + LIR block (metadata with)        
      
	
    if (page_tbl[ref_block].isHIR_block && (page_tbl[ref_block].recency == S_STACK_IN)){    //block is HIR block with metadata
      page_tbl[ref_block].isHIR_block = FALSE;
      num_LIR_pgs++;                  

      if (num_LIR_pgs > mem_size-HIR_block_portion_limit){   // LIR counter > stack S length    move maximum LIR block to stack Q top
	add_HIR_list_head(LIR_LRU_block_ptr);
	HIR_list_head->isHIR_block = TRUE;
	HIR_list_head->recency = S_STACK_OUT;
	num_LIR_pgs--; 
	LIR_LRU_block_ptr = find_last_LIR_LRU();
      }
      else 
	printf("Warning2!\n");
    }
    
    
    
    else if (page_tbl[ref_block].isHIR_block)                   //in stack Q 
      add_HIR_list_head((page_struct *)&page_tbl[ref_block]); 


    page_tbl[ref_block].recency = S_STACK_IN;                   //in stack S with metadata 

    prune_LIRS_stack();                                         //remove maximum HIR block metadata

    /*  To reduce the *.sln file size, ratios of stack size are 
     *  recorded every 10 references */  
    
    if (cur_lir_S_len>mem_size &&  /*total_pg_refs%5000 ==0 &&*/ sln_fp)
      fprintf(sln_fp, "%4lu %2.2f\n", total_pg_refs, (float)cur_lir_S_len/mem_size);      
    
    fscanf(trace_fp, "%lu", &ref_block);
  }

  return;
}


/* remove a block from memory */ 
int remove_LIRS_list(page_struct *page_ptr)
{ 
  if (!page_ptr)
    return FALSE;

  if (!page_ptr->LIRS_prev && !page_ptr->LIRS_next)
    return TRUE;

  if (page_ptr == LIR_LRU_block_ptr){              //if remove block is maximum recency LIR block, change LIR_LRU block
    LIR_LRU_block_ptr = page_ptr->LIRS_prev;
    LIR_LRU_block_ptr = find_last_LIR_LRU();
  }

  if (!page_ptr->LIRS_prev)                        //if remove block is top in stack S
    LRU_list_head = page_ptr->LIRS_next;
  else     
    page_ptr->LIRS_prev->LIRS_next = page_ptr->LIRS_next;   //remove midden block in stack S

  if (!page_ptr->LIRS_next)                                 //if remove bottom block in stack S
    LRU_list_tail = page_ptr->LIRS_prev; 
  else
    page_ptr->LIRS_next->LIRS_prev = page_ptr->LIRS_prev;   //remove midden block in stack S

  page_ptr->LIRS_prev = page_ptr->LIRS_next = NULL;
  return TRUE;
}

/* remove a block from its teh front of HIR resident list */
int remove_HIR_list(page_struct *HIR_block_ptr)
{
  if (!HIR_block_ptr)
    return FALSE;

  if (!HIR_block_ptr->HIR_rsd_prev)                  //stack Q only one HIR block or a top of stack Q
    HIR_list_head = HIR_block_ptr->HIR_rsd_next;
  else 
    HIR_block_ptr->HIR_rsd_prev->HIR_rsd_next = HIR_block_ptr->HIR_rsd_next;

  if (!HIR_block_ptr->HIR_rsd_next)                 //in stack Q bottom
    HIR_list_tail = HIR_block_ptr->HIR_rsd_prev; 
  else
    HIR_block_ptr->HIR_rsd_next->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_prev;

  HIR_block_ptr->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_next = NULL;   //close this block linklist

  return TRUE;
}

page_struct *find_last_LIR_LRU()
{

  if (!LIR_LRU_block_ptr){
    printf("Warning*\n");
    exit(1);
  }

  while (LIR_LRU_block_ptr->isHIR_block == TRUE){           //remove HIR blocks from stack S , prune stack S 
    LIR_LRU_block_ptr->recency = S_STACK_OUT;               //evict HIR block metadata
    cur_lir_S_len--;
    LIR_LRU_block_ptr = LIR_LRU_block_ptr->LIRS_prev;
  }    
 
  return LIR_LRU_block_ptr;
}

page_struct *prune_LIRS_stack()
{
  page_struct * tmp_ptr;
  int i = 0;

  if (cur_lir_S_len <=  MAX_S_LEN)
    return NULL;

  tmp_ptr = LIR_LRU_block_ptr;
  while (tmp_ptr->isHIR_block == 0)                       //remove maximum HIR block metadata
      tmp_ptr = tmp_ptr->LIRS_prev;

  tmp_ptr->recency = S_STACK_OUT;
  remove_LIRS_list(tmp_ptr);                      //remove from stack S , find maximm recency LIR block
  insert_LRU_list(tmp_ptr, LIR_LRU_block_ptr);
  cur_lir_S_len--;

  return tmp_ptr;
}


/* put a HIR resident block on the end of HIR resident list */ 
void add_HIR_list_head(page_struct * new_rsd_HIR_ptr)
{
  new_rsd_HIR_ptr->HIR_rsd_next = HIR_list_head;        //add block to stack Q , open rsd_HIR_next and rsd_HIR_prev linklist
  if (!HIR_list_head)
    HIR_list_tail = HIR_list_head = new_rsd_HIR_ptr;
  else
    HIR_list_head->HIR_rsd_prev = new_rsd_HIR_ptr;
  HIR_list_head = new_rsd_HIR_ptr;

  return;
}

/* put a newly referenced block on the top of LIRS stack */ 
void add_LRU_list_head(page_struct *new_ref_ptr)
{
  new_ref_ptr->LIRS_next = LRU_list_head; 

  if (!LRU_list_head){                    // first block add to stack S
    LRU_list_head = LRU_list_tail = new_ref_ptr;
    LIR_LRU_block_ptr = LRU_list_tail; /* since now the point to lir page with Smax isn't nil */ 
  } 
  else {
    LRU_list_head->LIRS_prev = new_ref_ptr;
    LRU_list_head = new_ref_ptr;
  }

  return;
}

/* insert a block in LIRS list */ 
void insert_LRU_list(page_struct *old_ref_ptr, page_struct *new_ref_ptr)
{
  old_ref_ptr->LIRS_next = new_ref_ptr->LIRS_next;
  old_ref_ptr->LIRS_prev = new_ref_ptr;
  
  if (new_ref_ptr->LIRS_next)
    new_ref_ptr->LIRS_next->LIRS_prev = old_ref_ptr;
  new_ref_ptr->LIRS_next = old_ref_ptr;
  
  return;
}

