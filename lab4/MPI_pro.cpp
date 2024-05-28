#include <windows.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <mpi.h>
#include <pmmintrin.h>
#include <omp.h>
using namespace std;


static const int thread_count = 4;



unsigned int Act[43577][1363] = { 0 };
unsigned int Pas[54274][1363] = { 0 };

const int Num = 1362;
const int pasNum = 54274;
const int lieNum = 43577;



//消元子初始化
void init_A()
{
    //每个消元子第一个为1位所在的位置，就是它所在二维数组的行号
    //例如：消元子（561，...）由Act[561][]存放
    unsigned int a;
    ifstream infile("act3.txt");
    char fin[10000] = { 0 };
    int index;
    //从文件中提取行
    while (infile.getline(fin, sizeof(fin)))
    {
        std::stringstream line(fin);
        int biaoji = 0;

        //从行中提取单个的数字
        while (line >> a)
        {
            if (biaoji == 0)
            {
                //取每行第一个数字为行标
                index = a;
                biaoji = 1;
            }
            int k = a % 32;
            int j = a / 32;

            int temp = 1 << k;
            Act[index][Num - 1 - j] += temp;
            Act[index][Num] = 1;//设置该位置记录消元子该行是否为空，为空则是0，否则为1
        }
    }
}

//被消元行初始化
void init_P()
{
    //直接按照磁盘文件的顺序存，在磁盘文件是第几行，在数组就是第几行
    unsigned int a;
    ifstream infile("pas3.txt");
    char fin[10000] = { 0 };
    int index = 0;
    //从文件中提取行
    while (infile.getline(fin, sizeof(fin)))
    {
        std::stringstream line(fin);
        int biaoji = 0;

        //从行中提取单个的数字
        while (line >> a)
        {
            if (biaoji == 0)
            {
                //用Pas[ ][263]存放被消元行每行第一个数字，用于之后的消元操作
                Pas[index][Num] = a;
                biaoji = 1;
            }

            int k = a % 32;
            int j = a / 32;

            int temp = 1 << k;
            Pas[index][Num - 1 - j] += temp;
        }
        index++;
    }
}



 

void f_ordinary()
{
    double seconds = 0;
    long long head, tail, freq;
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&head);//开始计时


    int i;
    for (i = lieNum - 1; i - 8 >= -1; i -= 8)
    {
        //每轮处理8个消元子，范围：首项在 i-7 到 i

        for (int j = 0; j < pasNum; j++)
        {
            //看4535个被消元行有没有首项在此范围内的
            while (Pas[j][Num] <= i && Pas[j][Num] >= i - 7)
            {
                int index = Pas[j][Num];
                if (Act[index][Num] == 1)//消元子不为空
                {
                    //Pas[j][]和Act[（Pas[j][18]）][]做异或
                    for (int k = 0; k < Num; k++)
                    {
                        Pas[j][k] = Pas[j][k] ^ Act[index][k];
                    }

                    //更新Pas[j][18]存的首项值
                    //做完异或之后继续找这个数的首项，存到Pas[j][18]，若还在范围里会继续while循环
                    //找异或之后Pas[j][ ]的首项
                    int num = 0, S_num = 0;
                    for (num = 0; num < Num; num++)
                    {
                        if (Pas[j][num] != 0)
                        {
                            unsigned int temp = Pas[j][num];
                            while (temp != 0)
                            {
                                temp = temp >> 1;
                                S_num++;
                            }
                            S_num += num * 32;
                            break;
                        }
                    }
                    Pas[j][Num] = S_num - 1;

                }
                else//消元子为空
                {
                    //Pas[j][]来补齐消元子
                    for (int k = 0; k < Num; k++)
                        Act[index][k] = Pas[j][k];

                    Act[index][Num] = 1;//设置消元子非空
                    break;
                }

            }
        }
    }


    for (int i = lieNum % 8 - 1; i >= 0; i--)
    {
        //每轮处理1个消元子，范围：首项等于i

        for (int j = 0; j < pasNum; j++)
        {
            //看53个被消元行有没有首项等于i的
            while (Pas[j][Num] == i)
            {
                if (Act[i][Num] == 1)//消元子不为空
                {
                    //Pas[j][]和Act[i][]做异或
                    for (int k = 0; k < Num; k++)
                    {
                        Pas[j][k] = Pas[j][k] ^ Act[i][k];
                    }

                    //更新Pas[j][18]存的首项值
                    //做完异或之后继续找这个数的首项，存到Pas[j][18]，若还在范围里会继续while循环
                    //找异或之后Pas[j][ ]的首项
                    int num = 0, S_num = 0;
                    for (num = 0; num < Num; num++)
                    {
                        if (Pas[j][num] != 0)
                        {
                            unsigned int temp = Pas[j][num];
                            while (temp != 0)
                            {
                                temp = temp >> 1;
                                S_num++;
                            }
                            S_num += num * 32;
                            break;
                        }
                    }
                    Pas[j][Num] = S_num - 1;

                }
                else//消元子为空
                {
                    //Pas[j][]来补齐消元子
                    for (int k = 0; k < Num; k++)
                        Act[i][k] = Pas[j][k];

                    Act[i][Num] = 1;//设置消元子非空
                    break;
                }
            }
        }
    }


    QueryPerformanceCounter((LARGE_INTEGER*)&tail);//结束计时
    seconds = (tail - head) * 1000.0 / freq;//单位 ms
    cout << "f_ordinary_pro: " << seconds << " ms" << endl;
}


void LU_pro(int rank, int num_proc)
{
    int i; // 声明变量 i

    // 循环开始，i 从 lieNum - 1 开始递减，每次递减 8，直到 i - 8 < -1 结束循环
    for (i = lieNum - 1; i - 8 >= -1; i -= 8)
    {
        // 内部循环，遍历 pasNum 个元素
        for (int j = 0; j < pasNum; j++)
        {
            // 检查当前进程是否需要处理该元素
            if (int(j % num_proc) == rank)
            {
                // 当 Pas[j][Num] 在区间 [i-7, i] 内时，执行循环
                while (Pas[j][Num] <= i && Pas[j][Num] >= i - 7)
                {
                    int index = Pas[j][Num]; // 获取 Pas[j][Num] 的值

                    // 检查 Act[index][Num] 是否为 1，即消元子是否为空
                    if (Act[index][Num] == 1)
                    {
                        // 如果消元子不为空，则执行下列操作

                        // 声明变量 k 和向量变量 va_Pas, va_Act
                        int k;
                        __m128 va_Pas, va_Act;

                        // 使用 SIMD 指令处理 Pas 和 Act 数组
                        for (k = 0; k + 4 <= Num; k += 4)
                        {
                            // 加载 Pas[j][k] 和 Act[index][k] 到向量寄存器中
                            va_Pas = _mm_loadu_ps((float*)&(Pas[j][k]));
                            va_Act = _mm_loadu_ps((float*)&(Act[index][k]));

                            // 对向量寄存器中的数据执行异或操作
                            va_Pas = _mm_xor_ps(va_Pas, va_Act);

                            // 将结果存储回 Pas 数组中
                            _mm_store_ss((float*)&(Pas[j][k]), va_Pas);
                        }

                        // 处理剩余不足 4 个元素的情况
                        for (; k < Num; k++)
                        {
                            Pas[j][k] = Pas[j][k] ^ Act[index][k];
                        }

                        // 计算新的 S_num 值
                        int num = 0, S_num = 0;
                        for (num = 0; num < Num; num++)
                        {
                            if (Pas[j][num] != 0)
                            {
                                unsigned int temp = Pas[j][num];
                                while (temp != 0)
                                {
                                    temp = temp >> 1;
                                    S_num++;
                                }
                                S_num += num * 32;
                                break;
                            }
                        }
                        Pas[j][Num] = S_num - 1;
                    }
                    else
                    {
                        // 如果消元子为空，则退出循环
                        break;
                    }
                }
            }
        }
    }

    // 处理剩余的行数
    for (int i = lieNum % 8 - 1; i >= 0; i--)
    {
        // 遍历 pasNum 个元素
        for (int j = 0; j < pasNum; j++)
        {
            // 检查当前进程是否需要处理该元素
            if (int(j % num_proc) == rank)
            {
                // 当 Pas[j][Num] 等于 i 时，执行循环
                while (Pas[j][Num] == i)
                {
                    // 检查 Act[i][Num] 是否为 1，即消元子是否为空
                    if (Act[i][Num] == 1)
                    {
                        // 如果消元子不为空，则执行下列操作

                        // 声明变量 k 和向量变量 va_Pas, va_Act
                        int k;
                        __m128 va_Pas, va_Act;

                        // 使用 SIMD 指令处理 Pas 和 Act 数组
                        for (k = 0; k + 4 <= Num; k += 4)
                        {
                            // 加载 Pas[j][k] 和 Act[i][k] 到向量寄存器中
                            va_Pas = _mm_loadu_ps((float*)&(Pas[j][k]));
                            va_Act = _mm_loadu_ps((float*)&(Act[i][k]));

                            // 对向量寄存器中的数据执行异或操作
                            va_Pas = _mm_xor_ps(va_Pas, va_Act);

                            // 将结果存储回 Pas 数组中
                            _mm_store_ss((float*)&(Pas[j][k]), va_Pas);
                        }

                        // 处理剩余不足 4 个元素的情况
                        for (; k < Num; k++)
                        {
                            Pas[j][k] = Pas[j][k] ^ Act[i][k];
                        }

                        // 计算新的 S_num 值
                        int num = 0, S_num = 0;
                        for (num = 0; num < Num; num++)
                        {
                            if (Pas[j][num] != 0)
                            {
                                unsigned int temp = Pas[j][num];
                                while (temp != 0)
                                {
                                    temp = temp >> 1;
                                    S_num++;
                                }
                                S_num += num * 32;
                                break;
                            }
                        }
                        Pas[j][Num] = S_num - 1;
                    }
                    else
                    {
                        // 如果消元子为空，则退出循环
                        break;
                    }
                }
            }
        }
    }
}



void f_mpi_pro()
{
    int num_proc; // 进程数
    int rank;     // 识别调用进程的 rank，值从 0 到 size-1

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc); // 获取进程数
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);     // 获取当前进程的 rank
    double seconds = 0;
    long long head, tail, freq;

    // 0 号进程——任务划分
    if (rank == 0)
    {
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        QueryPerformanceCounter((LARGE_INTEGER*)&head); // 开始计时
        int sign;
        do
        {
            // 任务划分
            for (int i = 0; i < pasNum; i++)
            {
                int flag = i % num_proc;
                if (flag == rank)
                    continue;
                else
                    MPI_Send(&Pas[i], Num + 1, MPI_FLOAT, flag, 0, MPI_COMM_WORLD);
            }
            LU_pro(rank, num_proc); // 执行消元操作
            // 处理完 0 号进程自己的任务后需接收其他进程处理之后的结果
            for (int i = 0; i < pasNum; i++)
            {
                int flag = i % num_proc;
                if (flag == rank)
                    continue;
                else
                    MPI_Recv(&Pas[i], Num + 1, MPI_FLOAT, flag, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            // 升格消元子，然后判断是否结束
            sign = 0;
            for (int i = 0; i < pasNum; i++)
            {
                // 找到第 i 个被消元行的首项
                int temp = Pas[i][Num];
                if (temp == -1)
                {
                    // 说明它已经被升格为消元子了
                    continue;
                }

                // 看这个首项对应的消元子是否为空，若为空，则补齐
                if (Act[temp][Num] == 0)
                {
                    // 补齐消元子
                    for (int k = 0; k < Num; k++)
                        Act[temp][k] = Pas[i][k];
                    // 将被消元行升格
                    Pas[i][Num] = -1;
                    // 标志设为 true，说明此轮还需继续
                    sign = 1;
                }
            }

            for (int r = 1; r < num_proc; r++)
            {
                MPI_Send(&sign, 1, MPI_INT, r, 2, MPI_COMM_WORLD);
            }
        } while (sign == 1);

        QueryPerformanceCounter((LARGE_INTEGER*)&tail); // 结束计时
        seconds += (tail - head) * 1000.0 / freq;       // 单位 ms
        cout << "f_mpi_pro: " << seconds << " ms" << endl;
    }

    // 其他进程
    else
    {
        int sign;

        do
        {
            // 非 0 号进程先接收任务
            for (int i = rank; i < pasNum; i += num_proc)
            {
                MPI_Recv(&Pas[i], Num + 1, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            // 执行任务
            LU_pro(rank, num_proc);
            // 非 0 号进程完成任务之后，将结果传回到 0 号进程
            for (int i = rank; i < pasNum; i += num_proc)
            {
                MPI_Send(&Pas[i], Num + 1, MPI_FLOAT, 0, 1, MPI_COMM_WORLD);
            }

            MPI_Recv(&sign, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } while (sign == 1);
    }
}





int main()
{
    init_A();
    init_P();
    f_ordinary();
 

    init_A();
    init_P();
    MPI_Init(NULL, NULL);

    f_mpi_pro();

    MPI_Finalize();

}
