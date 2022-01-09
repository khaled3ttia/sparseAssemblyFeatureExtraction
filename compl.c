int foo(int n, int m){

    int sum = 0;
    int c0;

    int a=1;

    for (c0 = n ; c0 > 0; c0-=a){
        a = a+1;
        int c1 = m;
        for (; c1 > (c0>5?c0:2*c0); c1--){
            sum += c0 > c1 ? 1 : 0;
        }

    }

    
    for (int i=0; i< 10; i++){
        for (int k=1; k < 100; k+=5){

            sum++;
        }

    }
    return sum;
}
