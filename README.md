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
