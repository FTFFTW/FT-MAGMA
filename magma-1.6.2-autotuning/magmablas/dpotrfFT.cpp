#include"FT.h"
#include<iostream>

using namespace std;
//Cholesky Factorization with FT on CPU using ACML
double get(double * matrix, int ld, int n, int i, int j) {
	if (i > ld || j > n)
		cout << "matrix_get_error" << endl;
	return *(matrix + j * ld + i);
}
/**
 * Cholesky factorization with FT on CPU using ACML
 * A: input matrix
 * lda: leading dimension of A
 * n: size of A
 * chksum1: checksum 1
 * inc1: stride between elememts in chksum1
 * chksum2: checksum 2
 * inc2: stride between elememts in chksum2
 */
void dpotrfFT(double * A, int lda, int n, int * info, 
			  ABFTEnv * abftEnv, bool FT , bool DEBUG, bool CHECK_BEFORE, bool CHECK_AFTER) {
	
	double one = 1;
	double zero = 0;
	double negone = -1;
	//cout << "potrf" << endl;
	
	if (FT && CHECK_BEFORE) {
		magma_set_lapack_numthreads(16);
		//verify A before use
		char T = 'T';
		double * chk1 = new double[n];
		double * chk2 = new double[n];
		int chk1_inc = 1;
		int chk2_inc = 1;
		blasf77_dgemv(  &T,
		                &n, &n,
		                &one,
		                A, &lda,
		                abftEnv->hrz_v, &(abftEnv->hrz_v_ld),
		                &zero,
		                chk1, &chk1_inc );
		blasf77_dgemv(  &T,
						&n, &n,
						&one,
						A, &lda,
						abftEnv->hrz_v + 1, &(abftEnv->hrz_v_ld),
						&zero,
						chk2, &chk2_inc ); 
		//handle error 
//		ErrorDetectAndCorrectHost(A, lda, n, n, n,
//								chksum, chksum_ld,
//								chk1, chk1_inc,
//								chk2, chk2_inc);
		
		if (DEBUG) {
			cout<<"[DPOTRF-BEFORE]recalcuated checksum on CPU before factorization:"<<endl;
			printMatrix_host(chk1, 1, 1, n, -1, -1);
			printMatrix_host(chk2, 1, 1, n, -1, -1);
			cout<<"[DPOTRF-BEFORE]updated checksum on CPU before factorization:"<<endl;
			printMatrix_host(abftEnv->col_hchk, abftEnv->col_hchk_ld, 2, n, -1, -1);
		}
	}
	
	//do Choleksy factorization
	magma_set_lapack_numthreads(1);
	char uplo = 'L';
	lapackf77_dpotrf(&uplo, &n, A, &n, info);


	if (FT) {
		//update checksum1 and checksum2

		magma_set_lapack_numthreads(64);
		for (int i = 0; i < n; i++) {
			//chksum1[i] = chksum1[i] / get(A, n, n, i, i);
			*(abftEnv->col_hchk + i*abftEnv->col_hchk_ld) = *(abftEnv->col_hchk + i*abftEnv->col_hchk_ld) / get(A, n, n, i, i);
			//(n-i-1, negone*chksum1[i], A + i*lda + i+1, 1, chksum1 + i+1, 1 );
			int m = n-i-1;
			double alpha = negone * (*(abftEnv->col_hchk + i * abftEnv->col_hchk_ld));
			int incx = 1;
			blasf77_daxpy(&m, &alpha, A + i*lda + i+1, &incx, abftEnv->col_hchk + (i+1) * abftEnv->col_hchk_ld, &(abftEnv->col_hchk_ld) );
		}
	
		for (int i = 0; i < n; i++) {
			//chksum2[i] = chksum2[i] / get(A, n, n, i, i);
			*(abftEnv->col_hchk + i*abftEnv->col_hchk_ld + 1) = *(abftEnv->col_hchk + i*abftEnv->col_hchk_ld + 1) / get(A, n, n, i, i);
			//daxpy(n-i-1, negone*chksum2[i], A + i*lda + i+1, 1, chksum2 + i+1, 1 );
			int m = n-i-1;
			double alpha = negone *  (*(abftEnv->col_hchk + i * abftEnv->col_hchk_ld + 1));
			int incx = 1;
			blasf77_daxpy(&m, &alpha, A + i * lda + i+1, &incx, abftEnv->col_hchk + 1 + (i + 1) * abftEnv->col_hchk_ld, &(abftEnv->col_hchk_ld) );
		}	
	}

	if (FT && CHECK_AFTER) {
		magma_set_lapack_numthreads(16);
		//verify A before use
		char T = 'T';
		double * chk1 = new double[n];
		double * chk2 = new double[n];
		int chk1_inc = 1;
		int chk2_inc = 1;
		blasf77_dgemv(  &T,
		                &n, &n,
		                &one,
		                A, &lda,
		                abftEnv->hrz_v, &(abftEnv->hrz_v_ld),
		                &zero,
		                chk1, &chk1_inc );
		blasf77_dgemv(  &T,
						&n, &n,
						&one,
						A, &lda,
						abftEnv->hrz_v + 1, &(abftEnv->hrz_v_ld),
						&zero,
						chk2, &chk2_inc ); 
		//handle error 
//		ErrorDetectAndCorrectHost(A, lda, n, n, n,
//								chksum, chksum_ld,
//								chk1, chk1_inc,
//								chk2, chk2_inc);
		
		if (DEBUG) {
			cout<<"[DPOTRF-BEFORE]recalcuated checksum on CPU before factorization:"<<endl;
			printMatrix_host(chk1, 1, 1, n, -1, -1);
			printMatrix_host(chk2, 1, 1, n, -1, -1);
			cout<<"[DPOTRF-BEFORE]updated checksum on CPU before factorization:"<<endl;
			printMatrix_host(abftEnv->col_hchk, abftEnv->col_hchk_ld, 2, n, -1, -1);
		}
	}
}