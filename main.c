int foo(int n, int m){

    int sum = 0;
    int c0;
    int b[10];
    int x[10];

    int a=1;

    for (c0 = n ; c0 > 0; c0-=2){
        a = a+1;
        int c1 = m;
        for (; c1 > 10; c1--){
            b[c1] += c0 > c1 ? x[c1] : 0;
        }

    }

    
    for (int i=0; i< m; i++){
        for (int k=0; i > k && k < m; k+=5){

            sum++;
        }

    }
    return sum;
}
