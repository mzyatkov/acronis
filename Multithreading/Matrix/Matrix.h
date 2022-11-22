#pragma once

#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <iomanip>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <cassert>

typedef double T;
class Matrix {
     public:
        Matrix(int, int);
        Matrix();
        ~Matrix();
        Matrix(const Matrix&);
        Matrix(const Matrix&, int, int, int);
        Matrix& operator=(const Matrix&);
        Matrix& operator*=(const Matrix&);
        bool operator==(const Matrix&);

        void add_block(const Matrix&, int,int);
        Matrix& block_prod_inplace(const Matrix&);
        friend std::ostream& operator<<(std::ostream&, const Matrix&);
        friend void multithread_naive_prod(Matrix&, const Matrix&, const Matrix&,const int, const int);
        friend void multithread_block_prod(Matrix&, const Matrix&, const Matrix&,const int, const int);
        friend Matrix multithread_execution(const Matrix&, const Matrix&, const int, const int );
        
        void fit_random();
        void swap_rows(int, int);
        Matrix transpose();

    private:
        int rows_, cols_;
        T **p;
        void allocSpace();
};

Matrix operator*(const Matrix&, const Matrix&);
Matrix block_prod(const Matrix&, const Matrix&);



