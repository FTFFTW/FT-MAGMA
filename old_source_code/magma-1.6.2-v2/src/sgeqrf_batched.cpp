/*
    -- MAGMA (version 1.6.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2011

       @generated from zgeqrf_batched.cpp normal z -> s, Fri Jan 30 19:00:19 2015
       @author Tingxing Dong
       @author Azzam Haidar
*/

#include "common_magma.h"
#include "batched_kernel_param.h"

#define PRECISION_s


/**
    Purpose
    -------
    SGEQRF computes a QR factorization of a real M-by-N matrix A:
    A = Q * R.
    
    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.

    @param[in,out]
    dA      REAL array on the GPU, dimension (LDDA,N)
            On entry, the M-by-N matrix A.
            On exit, the elements on and above the diagonal of the array
            contain the min(M,N)-by-N upper trapezoidal matrix R (R is
            upper triangular if m >= n); the elements below the diagonal,
            with the array TAU, represent the orthogonal matrix Q as a
            product of min(m,n) elementary reflectors (see Further
            Details).

    @param[in]
    ldda     INTEGER
            The leading dimension of the array dA.  LDDA >= max(1,M).
            To benefit from coalescent memory accesses LDDA must be
            divisible by 16.

    @param[out]
    tau     REAL array, dimension (min(M,N))
            The scalar factors of the elementary reflectors (see Further
            Details).


    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
                  or another error occured, such as memory allocation failed.

    Further Details
    ---------------
    The matrix Q is represented as a product of elementary reflectors

       Q = H(1) H(2) . . . H(k), where k = min(m,n).

    Each H(i) has the form

       H(i) = I - tau * v * v'

    where tau is a real scalar, and v is a real vector with
    v(1:i-1) = 0 and v(i) = 1; v(i+1:m) is stored on exit in A(i+1:m,i),
    and tau in TAU(i).

    @ingroup magma_sgeqrf_comp
    ********************************************************************/


extern "C" magma_int_t
magma_sgeqrf_batched(
    magma_int_t m, magma_int_t n, 
    float **dA_array,
    magma_int_t ldda, 
    float **tau_array,
    magma_int_t *info_array, magma_int_t batchCount, magma_queue_t queue)
{
#define dA(i, j)  (dA + (i) + (j)*ldda)   // A(i, j) means at i row, j column

    magma_int_t min_mn = min(m, n);
    cudaMemset(info_array, 0, batchCount*sizeof(magma_int_t));

    /* Check arguments */
    magma_int_t arginfo = 0;
    if (m < 0)
        arginfo = -1;
    else if (n < 0)
        arginfo = -2;
    else if (ldda < max(1,m))
        arginfo = -4;

    if (arginfo != 0) {
        magma_xerbla( __func__, -(arginfo) );
        return arginfo;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0)
        if(min_mn == 0 ) return arginfo;

    if( m >  2048 || n > 2048 ){
        printf("=========================================================================================\n");
        printf("   WARNING batched routines are designed for small sizes it might be better to use the\n   Native/Hybrid classical routines if you want performance\n");
        printf("=========================================================================================\n");
    }


    magma_int_t nb = 32;
    magma_int_t nnb = 8;
    magma_int_t i, k, ib=nb, jb=nnb;
    magma_int_t ldw, ldt, ldr, offset; 

    cublasHandle_t myhandle;
    cublasCreate_v2(&myhandle);


    float **dW0_displ = NULL;
    float **dW1_displ = NULL;
    float **dW2_displ = NULL;
    float **dW3_displ = NULL;
    float **dW4_displ = NULL;
    float **dW5_displ = NULL;

    float *dwork = NULL;
    float *dT   = NULL;
    float *dR   = NULL;
    float **dR_array = NULL;
    float **dT_array = NULL; 
    float **cpuAarray = NULL;
    float **cpuTarray = NULL;

    magma_malloc((void**)&dW0_displ, batchCount * sizeof(*dW0_displ));
    magma_malloc((void**)&dW1_displ, batchCount * sizeof(*dW1_displ));
    magma_malloc((void**)&dW2_displ, batchCount * sizeof(*dW2_displ));
    magma_malloc((void**)&dW3_displ, batchCount * sizeof(*dW3_displ));
    magma_malloc((void**)&dW4_displ, batchCount * sizeof(*dW4_displ));  // used in slarfb
    magma_malloc((void**)&dW5_displ, batchCount * sizeof(*dW5_displ));
    magma_malloc((void**)&dR_array, batchCount * sizeof(*dR_array));
    magma_malloc((void**)&dT_array, batchCount * sizeof(*dT_array));

    ldt = ldr = min(nb, min_mn);
    magma_smalloc(&dwork,  (2 * nb * n) * batchCount);
    magma_smalloc(&dR,  ldr * n   * batchCount);
    magma_smalloc(&dT,  ldt * ldt * batchCount);
    magma_malloc_cpu((void**) &cpuAarray, batchCount*sizeof(float*));
    magma_malloc_cpu((void**) &cpuTarray, batchCount*sizeof(float*));

    /* check allocation */
    if ( dW0_displ == NULL || dW1_displ == NULL || dW2_displ == NULL || dW3_displ == NULL || 
         dW4_displ == NULL || dW5_displ == NULL || dR_array  == NULL || dT_array  == NULL || 
         dR == NULL || dT == NULL || dwork == NULL || cpuAarray == NULL || cpuTarray == NULL ) {
        magma_free(dW0_displ);
        magma_free(dW1_displ);
        magma_free(dW2_displ);
        magma_free(dW3_displ);
        magma_free(dW4_displ);
        magma_free(dW5_displ);
        magma_free(dR_array);
        magma_free(dT_array);
        magma_free(dR);
        magma_free(dT);
        magma_free(dwork);
        free(cpuAarray);
        free(cpuTarray);
        magma_int_t info = MAGMA_ERR_DEVICE_ALLOC;
        magma_xerbla( __func__, -(info) );
        return info;
    }


    magmablas_slaset_q(MagmaFull, ldr, n*batchCount  , MAGMA_S_ZERO, MAGMA_S_ZERO, dR, ldr, queue);
    magmablas_slaset_q(MagmaFull, ldt, ldt*batchCount, MAGMA_S_ZERO, MAGMA_S_ZERO, dT, ldt, queue);
    sset_pointer(dR_array, dR, 1, 0, 0, ldr*min(nb, min_mn), batchCount, queue);
    sset_pointer(dT_array, dT, 1, 0, 0, ldt*min(nb, min_mn), batchCount, queue);


    magma_queue_t cstream;
    magmablasGetKernelStream(&cstream);
    magma_int_t streamid;
    const magma_int_t nbstreams=32;
    magma_queue_t stream[nbstreams];
    for(i=0; i<nbstreams; i++){
        magma_queue_create( &stream[i] );
    }
    magma_getvector( batchCount, sizeof(float*), dA_array, 1, cpuAarray, 1);
    magma_getvector( batchCount, sizeof(float*), dT_array, 1, cpuTarray, 1);


    magmablasSetKernelStream(NULL);

    for(i=0; i<min_mn;i+=nb)
    {
            ib = min(nb, min_mn-i);  

            //===============================================
            // panel factorization
            //===============================================

            magma_sdisplace_pointers(dW0_displ, dA_array, ldda, i, i, batchCount, queue); 
            magma_sdisplace_pointers(dW2_displ, tau_array, 1, i, 0, batchCount, queue);


            //dwork is used in panel factorization and trailing matrix update
            //dW4_displ, dW5_displ are used as workspace and configured inside
            magma_sgeqrf_panel_batched(m-i, ib, jb, 
                                       dW0_displ, ldda, 
                                       dW2_displ, 
                                       dT_array, ldt, 
                                       dR_array, ldr,
                                       dW1_displ,
                                       dW3_displ,
                                       dwork, 
                                       dW4_displ, dW5_displ,
                                       info_array,
                                       batchCount, myhandle, queue);
               
            //===============================================
            // end of panel
            //===============================================

           //direct panel matrix V in dW0_displ, 
           magma_sdisplace_pointers(dW0_displ, dA_array, ldda, i, i, batchCount, queue); 
           // copy the upper part of V into dR 
           sgeqrf_copy_upper_batched(ib, jb, dW0_displ, ldda, dR_array, ldr, batchCount, queue);
       
            //===============================================
            // update trailing matrix
            //===============================================

            //dwork is used in panel factorization and trailing matrix update
            //reset dW4_displ
            ldw = nb;
            sset_pointer(dW4_displ, dwork, 1, 0, 0,  ldw*n, batchCount, queue );
            offset = ldw*n*batchCount;
            sset_pointer(dW5_displ, dwork + offset, 1, 0, 0,  ldw*n, batchCount, queue );    

            if( (n-ib-i) > 0)
            {
    
                // set the diagonal of v as one and the upper triangular part as zero
                magmablas_slaset_batched(MagmaUpper, ib, ib, MAGMA_S_ZERO, MAGMA_S_ONE, dW0_displ, ldda, batchCount, queue); 
                magma_sdisplace_pointers(dW2_displ, tau_array, 1, i, 0, batchCount, queue); 

                // it is faster since it is using BLAS-3 GEMM routines, different from lapack implementation 
                magma_slarft_batched(m-i, ib, 0,
                                 dW0_displ, ldda,
                                 dW2_displ,
                                 dT_array, ldt, 
                                 dW4_displ, nb*ldt,
                                 batchCount, myhandle, queue);

                
                // perform C = (I-V T^H V^H) * C, C is the trailing matrix
                //-------------------------------------------
                //          USE STREAM  GEMM
                //-------------------------------------------
                if( (m-i) > 100  && (n-i-ib) > 100)   
                { 
                    // But since the code use the NULL stream everywhere, 
                    // so I don't need it, because the NULL stream do the sync by itself
                    //magma_device_sync(); 
                    for(k=0; k<batchCount; k++)
                    {
                        streamid = k%nbstreams;                                       
                        magmablasSetKernelStream(stream[streamid]);
                        
                        // the stream gemm must take cpu pointer 
                        magma_slarfb_gpu_gemm(MagmaLeft, MagmaConjTrans, MagmaForward, MagmaColumnwise,
                                    m-i, n-i-ib, ib,
                                    cpuAarray[k] + i + i * ldda, ldda, 
                                    cpuTarray[k], ldt,
                                    cpuAarray[k] + i + (i+ib) * ldda, ldda,
                                    dwork + nb * n * k, -1,
                                    dwork + nb * n * batchCount + nb * n * k, -1);
                      
                    }

                    // need to synchronise to be sure that panel does not start before
                    // finishing the update at least of the next panel
                    // BUT no need for it as soon as the other portion of the code 
                    // use the NULL stream which do the sync by itself 
                    //magma_device_sync(); 
                    magmablasSetKernelStream(NULL);
                }
                //-------------------------------------------
                //          USE BATCHED GEMM
                //-------------------------------------------
                else
                {
                    //direct trailing matrix in dW1_displ
                    magma_sdisplace_pointers(dW1_displ, dA_array, ldda, i, i+ib, batchCount, queue); 

                    magma_slarfb_gemm_batched(
                                MagmaLeft, MagmaConjTrans, MagmaForward, MagmaColumnwise, 
                                m-i, n-i-ib, ib,
                                (const float**)dW0_displ, ldda,
                                (const float**)dT_array, ldt,
                                dW1_displ,  ldda,
                                dW4_displ,  ldw,
                                dW5_displ, ldw,
                                batchCount, myhandle, queue);
               
                }

            }// update the trailing matrix 
            //===============================================

            // copy dR back to V after the trailing matrix update
            magmablas_slacpy_batched(MagmaUpper, ib, ib, dR_array, ldr, dW0_displ, ldda, batchCount, queue);

    }

    for(k=0; k<nbstreams; k++){
        magma_queue_destroy( stream[k] );
    }
    magmablasSetKernelStream(cstream);
    cublasDestroy_v2(myhandle);

    magma_free(dW0_displ);
    magma_free(dW1_displ);
    magma_free(dW2_displ);
    magma_free(dW3_displ);
    magma_free(dW4_displ);
    magma_free(dW5_displ);
    magma_free(dR_array);
    magma_free(dT_array);
    magma_free(dR);
    magma_free(dT);
    magma_free(dwork);
    free(cpuAarray);
    free(cpuTarray);

    return arginfo;
           
}





