#include"FT.h"
#include<iostream>
using namespace std;
//TRSM with FT on GPU using cuBLAS

/*
 * m: number of row of B
 * n: number of col of B
 */

void dtrsmFT(magma_side_t side, magma_uplo_t uplo, magma_trans_t trans, magma_diag_t diag,
		int m, int n, double alpha, double * A, int lda,
		double * B, int ldb, 
		ABFTEnv * abftEnv,
		double * checksumB, int checksumB_ld,
		bool FT, bool DEBUG, bool VERIFY, 
		magma_queue_t * stream) {

	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[4]);
	if (FT && VERIFY) {
		//verify B before use
		int mem_row = m; // number of row and col of B stored in memory(no trans operation)
		int mem_col = n;		
		// recalculateChecksum2(B, ldb,
		// 					mem_row, mem_col,
		// 					chk_nb,
		// 					vd, vd_ld,
		// 					chk1, chk1_ld,
		// 					chk2, chk2_ld,
		// 					streams);

		AutoTuneChecksumRecal(abftEnv, B, ldb, mem_row, mem_col, stream);

		if (DEBUG) {
			cudaStreamSynchronize(streams[1]);
			cout<<"[trsm] recalculated checksum of B before trsm:"<<endl;
			printMatrix_gpu(abftEnv->chk1, abftEnv->chk1_ld, mem_row / abftEnv->chk_nb, mem_col);
			printMatrix_gpu(abftEnv->chk2, abftEnv->chk2_ld, mem_row / abftEnv->chk_nb, mem_col);
		
			cout<<"[trsm] updated checksum of B before trsm:"<<endl;
			printMatrix_gpu(checksumB, checksumB_ld, (mem_row / abftEnv->chk_nb) * 2, mem_col);
		}

	}
	magmablasSetKernelStream(stream[1]);	
	//[Cholesky]MagmaRight, MagmaLower, MagmaTrans, MagmaNonUnit, MAGMA_D_ONE


	magma_dtrsm(side, uplo, trans, diag,
				m, n,
				alpha, A, lda,
			    B, ldb);


	if (FT) {
		//update checksums
		//magmablasSetKernelStream(stream[1]);	
		
		magmablasSetKernelStream(stream[4]);	
		magma_dtrsm(side, uplo, trans, diag,
                    (m / abftEnv->chk_nb) * 2, n,
                    alpha, A, lda,
                    checksumB, checksumB_ld);

		// cudaStreamSynchronize(streams[1]);
		// cudaStreamSynchronize(streams[4]);
		// //verify B before use
		// int mem_row = m; // number of row and col of B stored in memory(no trans operation)
		// int mem_col = n;		
		// recalculateChecksum(B, ldb,
		// 					mem_row, mem_col,
		// 					chk_nb,
		// 					vd, vd_ld,
		// 					chk1, chk1_ld,
		// 					chk2, chk2_ld,
		// 					stream);
		// if (DEBUG) {
		// 	cout<<"[trsm] recalculated checksum of B after trsm:"<<endl;
		// 	printMatrix_gpu(chk1, chk1_ld, mem_row / chk_nb, mem_col);
		// 	printMatrix_gpu(chk2, chk2_ld, mem_row / chk_nb, mem_col);
		
		// 	cout<<"[trsm] updated checksum of B after trsm:"<<endl;
		// 	printMatrix_gpu(checksumB, checksumB_ld, (mem_row / chk_nb) * 2, mem_col);
		// }
	}
}