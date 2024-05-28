#include <iostream>
#include <mpi.h>
#include <windows.h>
#include <pmmintrin.h>
#include <immintrin.h>
#include <omp.h>

using namespace std;

static const int N = 2000;
static const int NUM_THREADS = 4;

float arr[N][N];
float A[N][N];

void init_A(float arr[][N])
{
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            arr[i][j] = 0;
        }
        arr[i][i] = 1.0;
        for (int j = i + 1; j < N; j++)
            arr[i][j] = rand() % 100;
    }

    for (int i = 0; i < N; i++)
    {
        int k1 = rand() % N;
        int k2 = rand() % N;
        for (int j = 0; j < N; j++)
        {
            arr[i][j] += arr[0][j];
            arr[k1][j] += arr[k2][j];
        }
    }
}




void reset_A(float A[][N], float arr[][N])
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            A[i][j] = arr[i][j];
}


void f_ordinary()
{
    reset_A(A, arr);
    double seconds = 0;
    long long head, tail, freq, noww;
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时


    for (int k = 0; k < N; k++)
    {
        for (int j = k + 1; j < N; j++)
        {
            A[k][j] = A[k][j] * 1.0 / A[k][k];
        }
        A[k][k] = 1.0;

        for (int i = k + 1; i < N; i++)
        {
            for (int j = k + 1; j < N; j++)
            {
                A[i][j] = A[i][j] - A[i][k] * A[k][j];
            }
            A[i][k] = 0;
        }
    }
    QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时

    seconds += (tail - head) * 1000.0 / freq;//单位 ms

    cout << "f_ordinary: " << seconds << " ms" << endl;
}



void LU_block(float A[][N], int rank, int num_proc)
{
    // 计算每个进程负责的块大小以及剩余部分
    int block = N / num_proc;
    int remain = N % num_proc;

    // 确定每个进程处理的行范围
    int begin = rank * block;
    int end = rank != num_proc - 1 ? begin + block : begin + block + remain;

    // LU分解的主循环
    for (int k = 0; k < N; k++)
    {
        // 判断当前行是否由当前进程处理
        if (k >= begin && k < end)
        {
            // 对主元行进行归一化
            for (int j = k + 1; j < N; j++)
                A[k][j] = A[k][j] / A[k][k];
            A[k][k] = 1.0;

            // 将归一化后的行广播给所有其他进程
            for (int p = 0; p < num_proc; p++)
                if (p != rank)
                    MPI_Send(&A[k], N, MPI_FLOAT, p, 2, MPI_COMM_WORLD);
        }
        else
        {
            // 接收其他进程发送的归一化行
            int cur_p = k / block;
            MPI_Recv(&A[k], N, MPI_FLOAT, cur_p, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // 对当前进程负责的行进行消去操作
        for (int i = begin; i < end; i++)
        {
            if (i > k)
            {
                for (int j = k + 1; j < N; j++)
                    A[i][j] = A[i][j] - A[i][k] * A[k][j];
                A[i][k] = 0.0;
            }
        }
    }
}


void f_mpi_block()
{
    double seconds = 0;
    long long head, tail, freq, noww;

    int num_proc;//进程数
    int rank;//识别调用进程的rank，值从0~size-1

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int block = N / num_proc;   //块大小

    int remain = N % num_proc;  //余下的

    //0号进程——任务划分
    if (rank == 0)
    {
        reset_A(A, arr);


        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时
        //任务划分
        for (int i = 1; i < num_proc; i++)
        {
            if (i != num_proc - 1)
            {
                for (int j = 0; j < block; j++)
                    MPI_Send(&A[i * block + j], N, MPI_FLOAT, i, 0, MPI_COMM_WORLD);  //向进程发送数据
            }
            else
            {
                for (int j = 0; j < block + remain; j++)
                    MPI_Send(&A[i * block + j], N, MPI_FLOAT, i, 0, MPI_COMM_WORLD);
            }
        }
        LU_block(A, rank, num_proc);
        //处理完0号进程自己的任务后需接收其他进程处理之后的结果
        for (int i = 1; i < num_proc; i++)
        {
            if (i != num_proc - 1)
            {
                for (int j = 0; j < block; j++)
                    MPI_Recv(&A[i * block + j], N, MPI_FLOAT, i, 1,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            else
            {
                for (int j = 0; j < block + remain; j++)
                    MPI_Recv(&A[i * block + j], N, MPI_FLOAT, i, 1,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时

        seconds += (tail - head) * 1000.0 / freq;//单位 ms

        cout << "f_mpi_block: " << seconds << " ms" << endl;
        //print_A(A);
    }

    //其他进程
    else
    {
        //非0号进程先接收任务
        if (rank != num_proc - 1)
        {
            for (int j = 0; j < block; j++)
                MPI_Recv(&A[rank * block + j], N, MPI_FLOAT, 0, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else
        {
            for (int j = 0; j < block + remain; j++)
                MPI_Recv(&A[rank * block + j], N, MPI_FLOAT, 0, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        LU_block(A, rank, num_proc);
        //非0号进程完成任务之后，将结果传回到0号进程
        if (rank != num_proc - 1)
        {
            for (int j = 0; j < block; j++)
                MPI_Send(&A[rank * block + j], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
        else
        {
            for (int j = 0; j < block + remain; j++)
                MPI_Send(&A[rank * block + j], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
    }
}

void LU_block_sse(float A[][N], int rank, int num_proc)
{
    __m128 t1, t2, t3;
    int block = N / num_proc;  // 计算每个处理器处理的块大小
    int remain = N % num_proc; // 处理块划分后的剩余行数
    int begin = rank * block;  // 计算当前处理器的开始行
    int end = rank != num_proc - 1 ? begin + block : begin + block + remain; // 计算结束行，最后一个处理器包含剩余行


    {
        for (int k = 0; k < N; k++)
        {
            // 更新当前行
            if (k >= begin && k < end)
            {
                float temp1[4] = { A[k][k], A[k][k], A[k][k], A[k][k] };
                t1 = _mm_loadu_ps(temp1);


                for (int j = k + 1; j < N - 3; j += 4)
                {
                    t2 = _mm_loadu_ps(&A[k][j]);
                    t3 = _mm_div_ps(t2, t1);
                    _mm_storeu_ps(&A[k][j], t3);
                }

                {
                    for (int j = N - N % 4; j < N; j++)
                        A[k][j] = A[k][j] / A[k][k];
                    A[k][k] = 1.0;
                    for (int p = rank + 1; p < num_proc; p++)
                        MPI_Send(A[k], N, MPI_FLOAT, p, 2, MPI_COMM_WORLD);
                }
            }
            else
            {
                int cur_p = k / block;
                if (cur_p < rank)
                    MPI_Recv(A[k], N, MPI_FLOAT, cur_p, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            // 更新后续行

            for (int i = begin; i < end && i < N; i++)
            {
                if (i >= k + 1)
                {
                    float temp2[4] = { A[i][k], A[i][k], A[i][k], A[i][k] };
                    t1 = _mm_loadu_ps(temp2);

                    for (int j = k + 1; j <= N - 3; j += 4)
                    {
                        t2 = _mm_loadu_ps(&A[i][j]);
                        t3 = _mm_loadu_ps(&A[k][j]);
                        t3 = _mm_mul_ps(t1, t3);
                        t2 = _mm_sub_ps(t2, t3);
                        _mm_storeu_ps(&A[i][j], t2);
                    }
                    for (int j = N - N % 4; j < N; j++)
                        A[i][j] = A[i][j] - A[i][k] * A[k][j];
                    A[i][k] = 0;
                }
            }
        }
    }
}


void f_mpi_block_sse()
{
    double seconds = 0;
    long long head, tail, freq, noww;


    int num_proc;//进程数
    int rank;//识别调用进程的rank，值从0~size-1

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int block = N / num_proc;
    int remain = N % num_proc;
 
    //0号进程——任务划分
    if (rank == 0)
    {
        reset_A(A, arr);

        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时


        //任务划分
        for (int i = 1; i < num_proc; i++)
        {
            if (i != num_proc - 1)
            {
                for (int j = 0; j < block; j++)
                    MPI_Send(&A[i * block + j], N, MPI_FLOAT, i, 0, MPI_COMM_WORLD);
            }
            else
            {
                for (int j = 0; j < block + remain; j++)
                    MPI_Send(&A[i * block + j], N, MPI_FLOAT, i, 0, MPI_COMM_WORLD);
            }
        }
        LU_block_sse(A, rank, num_proc);
        //处理完0号进程自己的任务后需接收其他进程处理之后的结果
        for (int i = 1; i < num_proc; i++)
        {
            if (i != num_proc - 1)
            {
                for (int j = 0; j < block; j++)
                    MPI_Recv(&A[i * block + j], N, MPI_FLOAT, i, 1,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            else
            {
                for (int j = 0; j < block + remain; j++)
                    MPI_Recv(&A[i * block + j], N, MPI_FLOAT, i, 1,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
        seconds += (tail - head) * 1000.0 / freq;//单位 ms
        cout << "f_mpi_block_sse: " << seconds << " ms" << endl;
    }

    //其他进程
    else
    {
        //非0号进程先接收任务
        if (rank != num_proc - 1)
        {
            for (int j = 0; j < block; j++)
                MPI_Recv(&A[rank * block + j], N, MPI_FLOAT, 0, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else
        {
            for (int j = 0; j < block + remain; j++)
                MPI_Recv(&A[rank * block + j], N, MPI_FLOAT, 0, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        LU_block_sse(A, rank, num_proc);
        //处理完后向零号进程返回结果
        if (rank != num_proc - 1)
        {
            for (int j = 0; j < block; j++)
                MPI_Send(&A[rank * block + j], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
        else
        {
            for (int j = 0; j < block + remain; j++)
                MPI_Send(&A[rank * block + j], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
    }
}

void LU_block_sse_omp(float A[][N], int rank, int num_proc)
{
    __m128 t1, t2, t3;
    int block = N / num_proc;  // 计算每个处理器处理的块大小
    int remain = N % num_proc; // 处理块划分后的剩余行数
    int begin = rank * block;  // 计算当前处理器的开始行
    int end = rank != num_proc - 1 ? begin + block : begin + block + remain; // 计算结束行，最后一个处理器包含剩余行

#pragma omp parallel num_threads(NUM_THREADS)
    {
        for (int k = 0; k < N; k++)
        {
            // 更新当前行
            if (k >= begin && k < end)
            {
                float temp1[4] = { A[k][k], A[k][k], A[k][k], A[k][k] };
                t1 = _mm_loadu_ps(temp1);
#pragma omp for schedule(static)

                for (int j = k + 1; j < N - 3; j += 4)
                {
                    t2 = _mm_loadu_ps(&A[k][j]);
                    t3 = _mm_div_ps(t2, t1);
                    _mm_storeu_ps(&A[k][j], t3);
                }

                {
                    for (int j = N - N % 4; j < N; j++)
                        A[k][j] = A[k][j] / A[k][k];
                    A[k][k] = 1.0;
                    for (int p = rank + 1; p < num_proc; p++)
                        MPI_Send(A[k], N, MPI_FLOAT, p, 2, MPI_COMM_WORLD);
                }
            }
            else
            {
                int cur_p = k / block;
                if (cur_p < rank)
                    MPI_Recv(A[k], N, MPI_FLOAT, cur_p, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            // 更新后续行

            for (int i = begin; i < end && i < N; i++)
            {
                if (i >= k + 1)
                {
                    float temp2[4] = { A[i][k], A[i][k], A[i][k], A[i][k] };
                    t1 = _mm_loadu_ps(temp2);
#pragma omp for schedule(static)
                    for (int j = k + 1; j <= N - 3; j += 4)
                    {
                        t2 = _mm_loadu_ps(&A[i][j]);
                        t3 = _mm_loadu_ps(&A[k][j]);
                        t3 = _mm_mul_ps(t1, t3);
                        t2 = _mm_sub_ps(t2, t3);
                        _mm_storeu_ps(&A[i][j], t2);
                    }
                    for (int j = N - N % 4; j < N; j++)
                        A[i][j] = A[i][j] - A[i][k] * A[k][j];
                    A[i][k] = 0;
                }
            }
        }
    }
}


void f_mpi_block_sse_omp()
{
    double seconds = 0;
    long long head, tail, freq, noww;


    int num_proc;//进程数
    int rank;//识别调用进程的rank，值从0~size-1

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int block = N / num_proc;
    int remain = N % num_proc;

    //0号进程——任务划分
    if (rank == 0)
    {
        reset_A(A, arr);

        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时


        //任务划分
        for (int i = 1; i < num_proc; i++)
        {
            if (i != num_proc - 1)
            {
                for (int j = 0; j < block; j++)
                    MPI_Send(&A[i * block + j], N, MPI_FLOAT, i, 0, MPI_COMM_WORLD);
            }
            else
            {
                for (int j = 0; j < block + remain; j++)
                    MPI_Send(&A[i * block + j], N, MPI_FLOAT, i, 0, MPI_COMM_WORLD);
            }
        }
        LU_block_sse_omp(A, rank, num_proc);
        //处理完0号进程自己的任务后需接收其他进程处理之后的结果
        for (int i = 1; i < num_proc; i++)
        {
            if (i != num_proc - 1)
            {
                for (int j = 0; j < block; j++)
                    MPI_Recv(&A[i * block + j], N, MPI_FLOAT, i, 1,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            else
            {
                for (int j = 0; j < block + remain; j++)
                    MPI_Recv(&A[i * block + j], N, MPI_FLOAT, i, 1,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
        seconds += (tail - head) * 1000.0 / freq;//单位 ms
        cout << "f_mpi_block_sse: " << seconds << " ms" << endl;
    }

    //其他进程
    else
    {
        //非0号进程先接收任务
        if (rank != num_proc - 1)
        {
            for (int j = 0; j < block; j++)
                MPI_Recv(&A[rank * block + j], N, MPI_FLOAT, 0, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        else
        {
            for (int j = 0; j < block + remain; j++)
                MPI_Recv(&A[rank * block + j], N, MPI_FLOAT, 0, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        LU_block_sse_omp(A, rank, num_proc);
        //处理完后向零号进程返回结果
        if (rank != num_proc - 1)
        {
            for (int j = 0; j < block; j++)
                MPI_Send(&A[rank * block + j], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
        else
        {
            for (int j = 0; j < block + remain; j++)
                MPI_Send(&A[rank * block + j], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
    }
}




void LU_pipe(float A[][N], int rank, int num_proc) {
    // 定义前一进程和后一进程的编号
    int pre_proc = (rank + num_proc - 1) % num_proc;
    int next_proc = (rank + 1) % num_proc;

    // 主循环：遍历所有行
    for (int k = 0; k < N; k++) {
        // 判断当前行是否归当前进程处理
        if (k % num_proc == rank) {
            // 对角线元素归一化
            for (int j = k + 1; j < N; j++) {
                A[k][j] /= A[k][k];
            }
            A[k][k] = 1.0;

            // 处理完当前行后，向下一个进程发送当前行数据
            MPI_Send(&A[k], N, MPI_FLOAT, next_proc, 2, MPI_COMM_WORLD);
        }
        else {
            // 如果当前行不是由当前进程处理，接收前一个进程发送的数据
            MPI_Recv(&A[k], N, MPI_FLOAT, pre_proc, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // 如果下一个进程不是负责当前行，继续向下传递
            if ((k % num_proc) != next_proc) {
                MPI_Send(&A[k], N, MPI_FLOAT, next_proc, 2, MPI_COMM_WORLD);
            }
        }

        // 更新后续行的元素
        for (int i = k + 1; i < N; i++) {
            if (i % num_proc == rank) {
                for (int j = k + 1; j < N; j++) {
                    A[i][j] -= A[i][k] * A[k][j];
                }
                A[i][k] = 0.0;
            }
        }
    }
}


void f_mpi_pipe()
{

    double seconds = 0;
    long long head, tail, freq, noww;

    int num_proc;
    int rank;

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)
    {
        reset_A(A, arr);
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时
        //        在0号进程进行任务划分
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Send(&A[i], N, MPI_FLOAT, flag, 0, MPI_COMM_WORLD);
        }
        LU_pipe(A, rank, num_proc);
        //处理完0号进程自己的任务后需接收其他进程处理之后的结果
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Recv(&A[i], N, MPI_FLOAT, flag, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
        seconds += (tail - head) * 1000.0 / freq;//单位 ms
        cout << "f_mpi_pipe: " << seconds << " ms" << endl;
    }
    else
    {
        //非0号进程先接收任务
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Recv(&A[i], N, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        LU_pipe(A, rank, num_proc);
        //非0号进程完成任务之后，将结果传回到0号进程
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Send(&A[i], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
    }
}


void LU_pipe_noblocking(float A[][N], int rank, int num_proc, MPI_Request* send_request, MPI_Request* recv_request) {
    // 定义前一进程和后一进程的编号
    int pre_proc = (rank + num_proc - 1) % num_proc;
    int next_proc = (rank + 1) % num_proc;

    // 主循环：遍历所有行
    for (int k = 0; k < N; k++) {
        // 判断当前行是否归当前进程处理
        if (k % num_proc == rank) {
            // 对角线元素归一化
            for (int j = k + 1; j < N; j++) {
                A[k][j] /= A[k][k];
            }
            A[k][k] = 1.0;

            // 发送当前行数据给下一个进程（非阻塞）
            MPI_Isend(&A[k], N, MPI_FLOAT, next_proc, 2, MPI_COMM_WORLD, &send_request[k]);
        }
        else {
            // 如果当前行不是由当前进程处理，接收前一个进程发送的数据（非阻塞）
            MPI_Irecv(&A[k], N, MPI_FLOAT, pre_proc, 2, MPI_COMM_WORLD, &recv_request[k]);

            // 如果下一个进程不是负责当前行，继续向下传递
            if ((k % num_proc) != next_proc) {
                MPI_Isend(&A[k], N, MPI_FLOAT, next_proc, 2, MPI_COMM_WORLD, &send_request[k]);
            }
        }

        // 更新后续行的元素
        for (int i = k + 1; i < N; i++) {
            if (i % num_proc == rank) {
                for (int j = k + 1; j < N; j++) {
                    A[i][j] -= A[i][k] * A[k][j];
                }
                A[i][k] = 0.0;
            }
        }

       
    }
}


void f_mpi_pipe_noblocking()
{

    double seconds = 0;
    long long head, tail, freq, noww;

    int num_proc;
    int rank;

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Request* send_request = new MPI_Request[N];
    MPI_Request* recv_request = new MPI_Request[N];

    if (rank == 0)
    {
        reset_A(A, arr);
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时
        // 在0号进程进行任务划分
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Isend(&A[i], N, MPI_FLOAT, flag, 0, MPI_COMM_WORLD, &send_request[i]);
        }
        LU_pipe_noblocking(A, rank, num_proc, send_request, recv_request);
        // 处理完0号进程自己的任务后需接收其他进程处理之后的结果
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Irecv(&A[i], N, MPI_FLOAT, flag, 1, MPI_COMM_WORLD, &recv_request[i]);
        }

        // 等待通信完成
        MPI_Waitall(N, recv_request, MPI_STATUS_IGNORE);
        MPI_Waitall(N, send_request, MPI_STATUS_IGNORE);

        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
        seconds += (tail - head) * 1000.0 / freq;//单位 ms
        cout << "f_mpi_pipe_noblocking: " << seconds << " ms" << endl;
    }
    else
    {
        // 非0号进程先接收任务
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Irecv(&A[i], N, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, &recv_request[i]);
        }
        LU_pipe_noblocking(A, rank, num_proc, send_request, recv_request);
        // 非0号进程完成任务之后，将结果传回到0号进程
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Isend(&A[i], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD, &send_request[i]);
        }

        // 等待通信完成
        MPI_Waitall(N, recv_request, MPI_STATUS_IGNORE);
        MPI_Waitall(N, send_request, MPI_STATUS_IGNORE);
    }

    delete[] send_request;
    delete[] recv_request;
}


void LU_pipe_avx(float A[][N], int rank, int num_proc)
{
    __m256 t1, t2, t3;
    int pre_proc = (rank + (num_proc - 1)) % num_proc;
    int next_proc = (rank + 1) % num_proc;

    for (int k = 0; k < N; k++)
    {
        if (int(k % num_proc) == rank)
        {
            float temp1[8] = { A[k][k], A[k][k], A[k][k], A[k][k], A[k][k], A[k][k], A[k][k], A[k][k] };
            t1 = _mm256_loadu_ps(temp1);

            for (int j = k + 1; j < N - 7; j += 8)
            {
                t2 = _mm256_loadu_ps(A[k] + j);
                t3 = _mm256_div_ps(t2, t1);
                _mm256_storeu_ps(A[k] + j, t3);
            }
            for (int j = N - N % 8; j < N; j++)
            {
                A[k][j] = A[k][j] / A[k][k];
            }
            A[k][k] = 1.0;
            MPI_Send(&A[k], N, MPI_FLOAT, next_proc, 2, MPI_COMM_WORLD);
        }
        else
        {
            MPI_Recv(&A[k], N, MPI_FLOAT, pre_proc, 2,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (int(k % num_proc) != next_proc)
                MPI_Send(&A[k], N, MPI_FLOAT, next_proc, 2, MPI_COMM_WORLD);
        }
        for (int i = k + 1; i < N; i++)
        {
            if (int(i % num_proc) == rank)
            {
                float temp2[8] = { A[i][k], A[i][k], A[i][k], A[i][k], A[i][k], A[i][k], A[i][k], A[i][k] };
                t1 = _mm256_loadu_ps(temp2);

                for (int j = k + 1; j <= N - 7; j += 8)
                {
                    t2 = _mm256_loadu_ps(A[i] + j);
                    t3 = _mm256_loadu_ps(A[k] + j);
                    t3 = _mm256_mul_ps(t1, t3);
                    t2 = _mm256_sub_ps(t2, t3);
                    _mm256_storeu_ps(A[i] + j, t2);
                }
                for (int j = N - N % 8; j < N; j++)
                    A[i][j] = A[i][j] - A[i][k] * A[k][j];
                A[i][k] = 0;
            }
        }
    }
}


void f_mpi_pipe_avx()
{

    double seconds = 0;
    long long head, tail, freq, noww;

    int num_proc;
    int rank;

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)
    {
        reset_A(A, arr);
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时
        //        在0号进程进行任务划分
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Send(&A[i], N, MPI_FLOAT, flag, 0, MPI_COMM_WORLD);
        }
        LU_pipe_avx(A, rank, num_proc);
        //处理完0号进程自己的任务后需接收其他进程处理之后的结果
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Recv(&A[i], N, MPI_FLOAT, flag, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
        seconds += (tail - head) * 1000.0 / freq;//单位 ms
        cout << "f_mpi_pipe_avx: " << seconds << " ms" << endl;
    }
    else
    {
        //非0号进程先接收任务
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Recv(&A[i], N, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        LU_pipe_avx(A, rank, num_proc);
        //非0号进程完成任务之后，将结果传回到0号进程
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Send(&A[i], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
    }
}

void LU_pipe_opt(float A[][N], int rank, int num_proc)
{
    __m128 t1, t2, t3;
    int pre_proc = (rank + (num_proc - 1)) % num_proc;
    int next_proc = (rank + 1) % num_proc;
    //#pragma omp parallel num_threads(thread_count)
    for (int k = 0; k < N; k++)
    {
        if (int(k % num_proc) == rank)
        {
            float temp1[4] = { A[k][k], A[k][k], A[k][k], A[k][k] };
            t1 = _mm_loadu_ps(temp1);

            //#pragma omp for schedule(dynamic, 20)
            for (int j = k + 1; j < N - 3; j += 4)
            {
                t2 = _mm_loadu_ps(A[k] + j);
                t3 = _mm_div_ps(t2, t1);
                _mm_storeu_ps(A[k] + j, t3);
            }
            for (int j = N - N % 4; j < N; j++)
            {
                A[k][j] = A[k][j] / A[k][k];
            }
            A[k][k] = 1.0;
            MPI_Send(&A[k], N, MPI_FLOAT, next_proc, 2, MPI_COMM_WORLD);
        }
        else
        {
            MPI_Recv(&A[k], N, MPI_FLOAT, pre_proc, 2,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (int(k % num_proc) != next_proc)
                MPI_Send(&A[k], N, MPI_FLOAT, next_proc, 2, MPI_COMM_WORLD);
        }
        for (int i = k + 1; i < N; i++)
        {
            if (int(i % num_proc) == rank)
            {
                float temp2[4] = { A[i][k], A[i][k], A[i][k], A[i][k] };
                t1 = _mm_loadu_ps(temp2);
                //#pragma omp for schedule(dynamic, 20)
                for (int j = k + 1; j <= N - 3; j += 4)
                {
                    t2 = _mm_loadu_ps(A[i] + j);
                    t3 = _mm_loadu_ps(A[k] + j);
                    t3 = _mm_mul_ps(t1, t3);
                    t2 = _mm_sub_ps(t2, t3);
                    _mm_storeu_ps(A[i] + j, t2);
                }
                for (int j = N - N % 4; j < N; j++)
                    A[i][j] = A[i][j] - A[i][k] * A[k][j];
                A[i][k] = 0;
            }
        }
    }
}

void f_mpi_pipe_sse()
{
    double seconds = 0;
    long long head, tail, freq, noww;

    int num_proc;
    int rank;

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)
    {
        reset_A(A, arr);
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Send(&A[i], N, MPI_FLOAT, flag, 0, MPI_COMM_WORLD);
        }
        LU_pipe_opt(A, rank, num_proc);
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Recv(&A[i], N, MPI_FLOAT, flag, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
        seconds += (tail - head) * 1000.0 / freq;//单位 ms
        cout << "f_mpi_pipe_sse: " << seconds << " ms" << endl;
        //print_A(A);
    }
    else
    {
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Recv(&A[i], N, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        LU_pipe_opt(A, rank, num_proc);
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Send(&A[i], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
    }
}



void LU_recycle(float A[][N], int rank, int num_proc)
{
    for (int k = 0; k < N; k++)
    {
        //当前行是自己进程的任务——进行除法
        if (int(k % num_proc) == rank)
        {
            for (int j = k + 1; j < N; j++)
                A[k][j] = A[k][j] / A[k][k];
            A[k][k] = 1.0;
            //除法后向其他进程发送消息
            for (int p = 0; p < num_proc; p++)
                if (p != rank)
                    MPI_Send(&A[k], N, MPI_FLOAT, p, 2, MPI_COMM_WORLD);
        }
        //当前行不是自己进程的任务——接收消息
        else
        {
            MPI_Recv(&A[k], N, MPI_FLOAT, int(k % num_proc), 2,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        for (int i = k + 1; i < N; i++)
        {
            if (int(i % num_proc) == rank)
            {
                for (int j = k + 1; j < N; j++)
                    A[i][j] = A[i][j] - A[i][k] * A[k][j];
                A[i][k] = 0.0;
            }
        }
    }
}

void f_mpi_recycle()
{
    double seconds = 0;
    long long head, tail, freq;


    int num_proc;//进程数
    int rank;//识别调用进程的rank，值从0~size-1

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //0号进程——任务划分
    if (rank == 0)
    {
        reset_A(A, arr);
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时
        //任务划分
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Send(&A[i], N, MPI_FLOAT, flag, 0, MPI_COMM_WORLD);
        }
        LU_recycle(A, rank, num_proc);
        //处理完0号进程自己的任务后需接收其他进程处理之后的结果
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Recv(&A[i], N, MPI_FLOAT, flag, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
        seconds += (tail - head) * 1000.0 / freq;//单位 ms
        cout << "f_mpi_recycle: " << seconds << " ms" << endl;
    }
    //其他进程
    else
    {
        //非0号进程先接收任务
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Recv(&A[i], N, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        //执行任务
        LU_recycle(A, rank, num_proc);
        //非0号进程完成任务之后，将结果传回到0号进程
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Send(&A[i], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
    }
}

void LU_recycle_opt(float A[][N], int rank, int num_proc)
{
    __m128 t1, t2, t3;
    //#pragma omp parallel num_threads(thread_count)
    for (int k = 0; k < N; k++)
    {
        if (int(k % num_proc) == rank)
        {
            float temp1[4] = { A[k][k], A[k][k], A[k][k], A[k][k] };
            t1 = _mm_loadu_ps(temp1);
            //#pragma omp for schedule(dynamic, 20)
            for (int j = k + 1; j < N - 3; j += 4)
            {
                t2 = _mm_loadu_ps(A[k] + j);
                t3 = _mm_div_ps(t2, t1);
                _mm_storeu_ps(A[k] + j, t3);
            }
            for (int j = N - N % 4; j < N; j++)
            {
                A[k][j] = A[k][j] / A[k][k];
            }
            A[k][k] = 1.0;

            for (int p = 0; p < num_proc; p++)
                if (p != rank)
                    MPI_Send(&A[k], N, MPI_FLOAT, p, 2, MPI_COMM_WORLD);
        }
        else
        {
            MPI_Recv(&A[k], N, MPI_FLOAT, int(k % num_proc), 2,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        for (int i = k + 1; i < N; i++)
        {
            if (int(i % num_proc) == rank)
            {
                float temp2[4] = { A[i][k], A[i][k], A[i][k], A[i][k] };
                t1 = _mm_loadu_ps(temp2);
                //#pragma omp for schedule(dynamic, 20)
                for (int j = k + 1; j <= N - 3; j += 4)
                {
                    t2 = _mm_loadu_ps(A[i] + j);
                    t3 = _mm_loadu_ps(A[k] + j);
                    t3 = _mm_mul_ps(t1, t3);
                    t2 = _mm_sub_ps(t2, t3);
                    _mm_storeu_ps(A[i] + j, t2);
                }
                for (int j = N - N % 4; j < N; j++)
                    A[i][j] = A[i][j] - A[i][k] * A[k][j];
                A[i][k] = 0;
            }
        }
    }
}

void f_mpi_recycle_sse()
{
    double seconds = 0;
    long long head, tail, freq, noww;

    int num_proc;
    int rank;

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)
    {
        reset_A(A, arr);
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Send(&A[i], N, MPI_FLOAT, flag, 0, MPI_COMM_WORLD);
        }
        LU_recycle_opt(A, rank, num_proc);
        for (int i = 0; i < N; i++)
        {
            int flag = i % num_proc;
            if (flag == rank)
                continue;
            else
                MPI_Recv(&A[i], N, MPI_FLOAT, flag, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
        seconds += (tail - head) * 1000.0 / freq;//单位 ms
        cout << "f_mpi_recycle_sse: " << seconds << " ms" << endl;
        //print_A(A);
    }
    else
    {
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Recv(&A[i], N, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        LU_recycle_opt(A, rank, num_proc);
        for (int i = rank; i < N; i += num_proc)
        {
            MPI_Send(&A[i], N, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
        }
    }
}




int main()
{


    init_A(arr);

      f_ordinary();

    MPI_Init(NULL, NULL);


      f_mpi_block();
      f_mpi_block_sse();
      f_mpi_block_sse_omp();

      f_mpi_pipe();
      f_mpi_pipe_noblocking();
      f_mpi_pipe_sse();
      f_mpi_pipe_avx();

      f_mpi_recycle();
      f_mpi_recycle_sse();
    
    MPI_Finalize();

}
