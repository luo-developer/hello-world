
#ifndef QuasiFloat_h__
#define QuasiFloat_h__
typedef struct {
    int x;
    int e;
} QuasiFloat; //vaule = x*2^(30-e);
void IntegerQuasiFloat(int in, QuasiFloat *out, int e);
int QuasiFloatInteger(QuasiFloat *in, int e);
void QuasiFloatSub(QuasiFloat *r, const QuasiFloat *a, const QuasiFloat *b);
void QuasiFloatAdd(QuasiFloat *r, const QuasiFloat *a, const QuasiFloat *b);
void QuasiFloatAddInteger(QuasiFloat *r, const QuasiFloat *a, int b, int e);
void QuasiFloatMul(QuasiFloat *r, const QuasiFloat *a, const QuasiFloat *b);
void QuasiFloatDiv(QuasiFloat *r, const QuasiFloat *a, const QuasiFloat *b);
void QuasiFloatAbs(QuasiFloat *x);
void QuasiFloatNeg(QuasiFloat *a, QuasiFloat *b);
int QuasiFloatGetSign(QuasiFloat *a);
int QuasiFloatCmp(const QuasiFloat *a, const QuasiFloat *b);
void QuasiFloatLog2(QuasiFloat *r, const QuasiFloat *x);
void QuasiFloatPow2(QuasiFloat *r, QuasiFloat *x);
void QuasiFloatLn(QuasiFloat *r, QuasiFloat *x);
void QuasiFloatExp(QuasiFloat *r, QuasiFloat *x);
void QuasiFloatInv(QuasiFloat *r, QuasiFloat *x);
void QuasiFloatLog10(QuasiFloat *r, QuasiFloat *x);
void QuasiFloatPow10(QuasiFloat *r, QuasiFloat *x);
#endif // QuasiFloat_h__

