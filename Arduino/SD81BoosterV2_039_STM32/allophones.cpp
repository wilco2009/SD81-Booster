#include "allophones.h"

const uint8_t *allophone_data_pointers[64]={a00pa1,a01pa2,a02pa3,a03pa4,a04pa5,a05oy,a06ay,a07eh,a08kk3,a09pp,a10jh,a11nn1,a12ih,a13tt2,a14rr1,a15ax,a16mm,a17tt1,a18dh1,a19iy,a20ey,a21dd1,a22uw1,a23ao,a24aa,a25yy2,a26ae,a27hh1,a28bb1,a29th,a30uh,a31uw2,a32aw,a33dd2,a34gg3,a35vv,a36gg1,a37sh,a38zh,a39rr2,a40ff,a41kk2,a42kk1,a43zz,a44ng,a45ll,a46ww,a47xr,a48wh,a49yy1,a50ch,a51er1,a52er2,a53ow,a54dh2,a55ss,a56nn2,a57hh2,a58or,a59ar,a60yr,a61gg2,a62el,a63bb2,};

//Sizes of arrays above
//---------------------------------------------------------------

//const int allophonesize[64]={57,510,510,1052,2175,4548,2857,760,1334,532,1512,1472,750,1465,1848,743,1976,1078,1817,2728,3076,810,1110,1078,1100,2012,1258,2448,892,1994,1102,2840,4052,1657,1525,2016,802,1712,2100,1344,1541,1977,1579,2258,2440,1208,1934,3940,2140,1403,1996,1718,3289,2559,2610,940,2111,845,3612,3122,3795,419,2040,601,};


//Corrected (for time) Sizes of arrays above
//---------------------------------------------------------------

// Array con los tamaños de las muestras de cada alófono
// ¡Asegúrate de que este orden coincide con allophone_index!
const unsigned int allophone_sizes[64] = {
    sizeof(a00pa1), sizeof(a01pa2), sizeof(a02pa3), sizeof(a03pa4), sizeof(a04pa5), sizeof(a05oy), sizeof(a06ay), sizeof(a07eh), sizeof(a08kk3), sizeof(a09pp),
    sizeof(a10jh), sizeof(a11nn1), sizeof(a12ih), sizeof(a13tt2), sizeof(a14rr1), sizeof(a15ax), sizeof(a16mm), sizeof(a17tt1), sizeof(a18dh1), sizeof(a19iy),
    sizeof(a20ey), sizeof(a21dd1), sizeof(a22uw1), sizeof(a23ao), sizeof(a24aa), sizeof(a25yy2), sizeof(a26ae), sizeof(a27hh1), sizeof(a28bb1), sizeof(a29th),
    sizeof(a30uh), sizeof(a31uw2), sizeof(a32aw), sizeof(a33dd2), sizeof(a34gg3), sizeof(a35vv), sizeof(a36gg1), sizeof(a37sh), sizeof(a38zh), sizeof(a39rr2),
    sizeof(a40ff), sizeof(a41kk2), sizeof(a42kk1), sizeof(a43zz), sizeof(a44ng), sizeof(a45ll), sizeof(a46ww), sizeof(a47xr), sizeof(a48wh), sizeof(a49yy1),
    sizeof(a50ch), sizeof(a51er1), sizeof(a52er2), sizeof(a53ow), sizeof(a54dh2), sizeof(a55ss), sizeof(a56nn2), sizeof(a57hh2), sizeof(a58or), sizeof(a59ar),
    sizeof(a60yr), sizeof(a61gg2), sizeof(a62el), 0 // Placeholder para el índice 63 si hay menos de 64 definidos, o simplemente lista los 63 reales
};
