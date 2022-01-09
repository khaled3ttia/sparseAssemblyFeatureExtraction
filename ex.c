void another(){

    int a[10];
    for (int x = 0; x < 10; x++){
        a[x] = x;
    }


    int othercount = 0;
    for (int y= 0 ; y < 12; y++){
        if ( y % 10 == 1){
            break;
        }
        othercount++;
    }

}


int main(){

    int somecount = 0;
    for (int i = 0; i< 10; i++){
        if (i%2 == 0){
            somecount++;
        }else{
            somecount--;
        }

    }
    return 0;
}
