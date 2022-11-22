#include "Matrix.h"

using namespace std;

constexpr int MIN = -1;
constexpr int MAX = 1;
constexpr int MAX_OUTPUT_SIZE = 10;
constexpr int MAX_THREADS = 30;
constexpr double EPS = 1e-9;
constexpr int MATRIX_SIZE = 2048;
constexpr int BLOCK_SIZE = 8;

Matrix::Matrix(int rows, int cols) : rows_(rows), cols_(cols)
{
    allocSpace();

    for (int i = 0; i < rows_; ++i)
    {
        for (int j = 0; j < cols_; ++j)
        {
            p[i][j] = 0;
        }
    }
}

void Matrix::fit_random()
{
    std::random_device rd;
    std::default_random_engine eng(rd());
    std::uniform_real_distribution<double> distr(MIN, MAX);

    for (int i = 0; i < rows_; ++i)
    {
        for (int j = 0; j < cols_; ++j)
        {
            p[i][j] = distr(eng);
        }
    }
}

Matrix::Matrix() : rows_(1), cols_(1)
{
    allocSpace();
    p[0][0] = 0;
}
Matrix::Matrix(const Matrix &m) : rows_(m.rows_), cols_(m.cols_)
{
    allocSpace();
    for (int i = 0; i < rows_; ++i)
    {
        for (int j = 0; j < cols_; ++j)
        {
            p[i][j] = m.p[i][j];
        }
    }
}
Matrix::Matrix(const Matrix &m, int size, int i, int j) : rows_(size), cols_(size)
{
    if (size + i > m.rows_ || size + j > m.cols_)
    {
        throw std::invalid_argument("block is out of bounds for the matrix");
    }
    allocSpace();
    for (int ii = 0; ii < rows_; ++ii)
    {
        for (int jj = 0; jj < cols_; ++jj)
        {
            p[ii][jj] = m.p[i + ii][j + jj];
        }
    }
}
Matrix::~Matrix()
{
    for (int i = 0; i < rows_; ++i)
    {
        delete[] p[i];
    }
    delete[] p;
}

Matrix &Matrix::operator=(const Matrix &m)
{
    if (this == &m)
    {
        return *this;
    }

    if (rows_ != m.rows_ || cols_ != m.cols_)
    {
        for (int i = 0; i < rows_; ++i)
        {
            delete[] p[i];
        }
        delete[] p;

        rows_ = m.rows_;
        cols_ = m.cols_;
        allocSpace();
    }

    for (int i = 0; i < rows_; ++i)
    {
        for (int j = 0; j < cols_; ++j)
        {
            p[i][j] = m.p[i][j];
        }
    }
    return *this;
}

void Matrix::allocSpace()
{
    p = new T *[rows_];
    for (int i = 0; i < rows_; ++i)
    {
        p[i] = new T[cols_];
    }
}
bool Matrix::operator==(const Matrix &r)
{
    if (rows_ != r.rows_ || cols_ != r.cols_)
    {
        return false;
    }
    for (int i = 0; i < rows_; ++i)
    {
        for (int j = 0; j < cols_; ++j)
        {
            if (abs(p[i][j] - r.p[i][j]) > EPS)
            {
                return false;
            }
        }
    }
    return true;
}

Matrix &Matrix::operator*=(const Matrix &m)
{
    Matrix temp(rows_, m.cols_);
    for (int i = 0; i < temp.rows_; ++i)
    {
        for (int j = 0; j < temp.cols_; ++j)
        {
            for (int k = 0; k < cols_; ++k)
            {
                temp.p[i][j] += (p[i][k] * m.p[k][j]);
            }
        }
    }
    return (*this = temp);
}

void Matrix::add_block(const Matrix &block, int i, int j)
{
    if (block.rows_ + i > rows_ || block.cols_ + j > cols_)
    {
        throw std::invalid_argument("block is out of bounds for the matrix");
    }
    for (int ii = 0; ii < block.rows_; ++ii)
    {
        for (int jj = 0; jj < block.cols_; ++jj)
        {
            p[i + ii][j + jj] += block.p[ii][jj];
        }
    }
}
Matrix &Matrix::block_prod_inplace(const Matrix &B)
{
    assert(rows_ == cols_ && B.rows_ == B.cols_ && rows_ == B.rows_);
    int N = rows_;
    assert(N % BLOCK_SIZE == 0);
    int block_num = N / BLOCK_SIZE;
    Matrix C(N, N);

    for (int jj = 0; jj < N; jj += BLOCK_SIZE)
    {
        for (int kk = 0; kk < N; kk += BLOCK_SIZE)
        {
            for (int i = 0; i < N; i++)
            {
                for (int j = jj; j < (jj + BLOCK_SIZE); j++)
                {
                    T temp = 0;
                    for (int k = kk; k < (kk + BLOCK_SIZE); k++)
                    {
                        temp += p[i][k] * B.p[k][j];
                    }
                    C.p[i][j] += temp;
                }
            }
        }
    }

    return (*this = C);
}

// NON MEMBER FUNCTIONS
Matrix operator*(const Matrix &A, const Matrix &B)
{
    Matrix tmp(A);
    return (tmp *= B);
}

Matrix block_prod(const Matrix &A, const Matrix &B)
{
    Matrix tmp(A);
    return (tmp.block_prod_inplace(B));
}

void multithread_naive_prod(Matrix &C, const Matrix &A, const Matrix &B,
                            const int thread_number, const int threads_number)
{
    // Calculate workload
    const int n_elements = (MATRIX_SIZE * MATRIX_SIZE);
    const int n_operations = n_elements / threads_number;
    const int rest_operations = n_elements % threads_number;

    int start_op, end_op;

    if (thread_number == 0)
    {
        // First thread does more job
        start_op = n_operations * thread_number;
        end_op = (n_operations * (thread_number + 1)) + rest_operations;
    }
    else
    {
        start_op = n_operations * thread_number + rest_operations;
        end_op = (n_operations * (thread_number + 1)) + rest_operations;
    }

    for (int op = start_op; op < end_op; ++op)
    {
        const int row = op % MATRIX_SIZE;
        const int col = op / MATRIX_SIZE;
        T r = 0;
        for (int i = 0; i < MATRIX_SIZE; ++i)
        {
            const T e1 = A.p[row][i];
            const T e2 = B.p[i][col];
            r += e1 * e2;
        }

        C.p[row][col] = r;
    }
}

void multithread_block_prod(Matrix &C, const Matrix &A, const Matrix &B,
                            const int thread_number, const int threads_number)
{
    // Calculate workload
    const int n_elements = (MATRIX_SIZE * MATRIX_SIZE) / (BLOCK_SIZE * BLOCK_SIZE);
    const int n_operations = n_elements / threads_number;
    const int rest_operations = n_elements % threads_number;
    const int n_blocks = MATRIX_SIZE / BLOCK_SIZE;

    int start_op, end_op;

    if (thread_number == 0)
    {
        // First thread does more job
        start_op = n_operations * thread_number;
        end_op = (n_operations * (thread_number + 1)) + rest_operations;
    }
    else
    {
        start_op = n_operations * thread_number + rest_operations;
        end_op = (n_operations * (thread_number + 1)) + rest_operations;
    }

    // for (int op = start_op; op < end_op; ++op)
    // {
    //     const int row_block = (op / n_blocks) * block_size;
    //     const int col_block = (op % n_blocks) * block_size;
    //     T r = 0;
    //     for (int i = 0; i < MATRIX_SIZE; i+= block_size)
    //     {
    //        Matrix blockA = Matrix(A, block_size, row_block, i);
    //        Matrix blockB = Matrix(B, block_size, i, col_block);
    //        blockA *= blockB;
    //        C.add_block(blockA, row_block, col_block);
    //     }
    // }

    for (int op = start_op; op < end_op; ++op)
    {
        const int row_block = (op / n_blocks) * BLOCK_SIZE;
        const int col_block = (op % n_blocks) * BLOCK_SIZE;
        for (int i = 0; i < MATRIX_SIZE; i++)
        {
            for (int j = row_block; j < (row_block + BLOCK_SIZE); j++)
            {
                T temp = 0;
                for (int k = col_block; k < (col_block + BLOCK_SIZE); k++)
                {
                    temp += A.p[i][k] * B.p[k][j];
                }
                C.p[i][j] += temp;
            }
        }
    }
}

Matrix multithread_execution(const Matrix &A, const Matrix &B, const int threads_number, const int method)
{
    // std::cout << "Starting multithreading execution..." << std::endl;
    void (*function)(Matrix &, const Matrix &, const Matrix &, const int, const int);
    if (method == 0)
    {
        function = multithread_naive_prod;
    }
    else if (method == 1)
    {
        function = multithread_block_prod;
    }
    else
    {
        assert(0);
    }
    thread *threads = new thread[threads_number];
    Matrix C(A.rows_, B.cols_);

    for (int i = 0; i < threads_number; ++i)
    {
        // std::cout << "Starting thread " << i << std::endl;
        threads[i] = std::thread(function, std::ref(C), std::cref(A), std::cref(B), i, threads_number);
    }

    // std::cout << "Calculating...." << std::endl;

    for (int i = 0; i < threads_number; ++i)
    {
        // std::cout << "Joining thread " << i << std::endl;
        threads[i].join();
    }

    delete[] threads;
    return C;
    // std::cout << "Finishing multithreading execution..." << std::endl;
}

ostream &operator<<(ostream &os, const Matrix &m)
{
    for (int i = 0; i < min(m.rows_, MAX_OUTPUT_SIZE); ++i)
    {
        for (int j = 0; j < min(m.cols_, MAX_OUTPUT_SIZE); ++j)
        {
            os << setprecision(2) << setw(8) << m.p[i][j];
        }
        os << endl;
    }
    return os;
}

void get_graphic_data(int max_size)
{
    ofstream outfile;
    outfile.open("onethread_naive.dat", ios::out | ios::trunc);
    outfile << "matrix_size,time_sec" << endl;
    for (int i = 1; i < max_size; i += 1)
    {
        Matrix A(i, i);
        Matrix B(i, i);
        A.fit_random();
        B.fit_random();
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        A *= B;
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        outfile << i << "," << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1e6 << endl;
    }
}

void get_graphic_data2(int max_size)
{
    ofstream outfile;
    outfile.open("onethread_block.dat", ios::out | ios::trunc);

    outfile << "matrix_size,time_sec" << endl;
    for (int i = BLOCK_SIZE; i < max_size; i += BLOCK_SIZE)
    {
        Matrix A(i, i);
        Matrix B(i, i);
        A.fit_random();
        B.fit_random();

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        A.block_prod_inplace(B);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        outfile << i << "," << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1e6 << endl;
    }
}

void get_graphic_data3()
{
    ofstream outfile;
    outfile.open("multithread_naive.dat", ios::out | ios::trunc);

    outfile << "thread_number,time_sec" << endl;
    for (int threads_number = 1; threads_number < MAX_THREADS; threads_number++)
    {
        Matrix A(MATRIX_SIZE, MATRIX_SIZE);
        Matrix B(MATRIX_SIZE, MATRIX_SIZE);
        A.fit_random();
        B.fit_random();

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        Matrix C = multithread_execution(A, B, threads_number, 0);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        outfile << threads_number << "," << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1e6 << endl;
    }
}
void get_graphic_data4()
{
    ofstream outfile;
    outfile.open("multithread_block.dat", ios::out | ios::trunc);

    outfile << "thread_number,time_sec" << endl;
    for (int threads_number = 1; threads_number < MAX_THREADS; threads_number++)
    {
        Matrix A(MATRIX_SIZE, MATRIX_SIZE);
        Matrix B(MATRIX_SIZE, MATRIX_SIZE);
        A.fit_random();
        B.fit_random();

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        Matrix C = multithread_execution(A, B, threads_number, 1);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        outfile << threads_number << "," << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1e6 << endl;
    }
}

void check_correctness(int threads_num)
{
    assert(MATRIX_SIZE % BLOCK_SIZE == 0);
    Matrix A(MATRIX_SIZE, MATRIX_SIZE);
    Matrix B(MATRIX_SIZE, MATRIX_SIZE);
    A.fit_random();
    B.fit_random();
    Matrix C = A * B;
    Matrix D = block_prod(A, B);
    Matrix E = multithread_execution(A, B, threads_num, 0);
    Matrix F = multithread_execution(A, B, threads_num, 1);
    // cout << A << endl
    //      << B << endl
    //      << C << endl
    //      << D << endl
    //      << E << endl
    //      << F << endl;
    assert(C == D);
    assert(C == E);
    assert(E == D);
    assert(F == C);
}
int main()
{
    // Matrix A(SIZE_M, SIZE_N);
    // Matrix B(SIZE_N, SIZE_K);
    // A.fit_random();
    // B.fit_random();

    // std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    // Matrix C = A * B;
    // std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    // std::cout << "Time naive prod = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1e6 << "[s]" << std::endl;

    // check_correctness(8);
    get_graphic_data(500);
    get_graphic_data2(500);
    // get_graphic_data3();
    // get_graphic_data4();
}