#if !defined(HANDMADE_MATH_H)

struct v2
{
    real32 X, Y;
    real32 &operator[](int Index) { return ((&X)[Index]); }

    inline v2& operator*=(real32 A);
    inline v2& operator+=(v2 A);
};

inline v2
operator*(real32 A, v2 B)
{
    v2 Result;

    Result.X = A * B.X;
    Result.Y = A * B.Y;

    return Result;
}

inline v2
operator+(v2 A, v2 B)
{
    v2 Result;

    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;

    return Result;
}

inline v2
operator-(v2 A, v2 B)
{
    v2 Result;

    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;

    return Result;
}

inline v2
operator-(v2 A)
{
    v2 Result;

    Result.X = -A.X;
    Result.Y = -A.Y;

    return Result;
}

inline v2&
v2::operator*=(real32 A)
{
    *this = A * *this;

    return *this;
}

inline v2&
v2::operator+=(v2 A)
{
    *this = *this + A;

    return *this;
}

#define HANDMADE_MATH_H
#endif