
#define FIFO_FILE "MYFIFO"
#define MEM_PARTILHADA "organizacao.shm"
#define COMPRIMENTO_MENSAGEM 40
#define TAMANHO_NOME 20
#define USER_LEVEL_PERMISSIONS 0666
#define TEM_ERRO -1
#define NUM_CARROS 9
#define MASTER_FIFO "MASTER.in"
#define COMPRIMENTO_MENSAGEM_MASTER 9

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <semaphore.h>

typedef struct StatusCarro{
    short id;					/* Numero de carro */
    int volta;              /* Volta na pista */
    int setor;
    float tempo;
    int combustivel;
    int pneus;
    char tipo_pneu[TAMANHO_NOME];          /* tipo pneu */
} Status;

typedef struct OrganizacaoCorrida{
    short id;					/* Numero de carro */
    char nome_equipa[TAMANHO_NOME];          /* Nome da equipa do carro */
    char nome_piloto[TAMANHO_NOME];          /* Nome da equipa do carro */
    int id_equipa; /* Id unico da equipa*/
    int pneus_disponiveis; /* pneus disponives */
} Organizacao;


void * criaMemoriaPartilhada(char * nome, int tamanho)
{
    void *ptr = NULL;
    int ret = 0;
    int fd = 0;

    /* Cria memoria partilhada */
    fd = shm_open(nome , O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("Erro a usar shm_open");
        exit(1);
    }

    /* Define tamanho da memoria partilhada */
    ret = ftruncate(fd, tamanho);
    if (ret == -1)
    {
        perror("Erro a usar ftruncate");
        exit(2);
    }

    /* Associa memoria partilhada ao ponteiro ptr */
    ptr = mmap(0, tamanho, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        perror("shm-mmap");
        exit(3);
    }

    return ptr;
}

void apagaMemoriaPartilhada(char * nome, int tamanho, void * ptr)
{
    int ret = 0;

    ret = munmap(ptr, tamanho);

    if (ret == -1)
    {
        perror("Erro na funcao munmap");
        exit(7);
    }

    ret = shm_unlink(nome);
    if (ret == -1)
    {
        perror("Erro na funcao shm_unlink");
        exit(8);
    }
}

int buscaPneusDisponiveis(int id)
{
    char nome[TAMANHO_NOME] = MEM_PARTILHADA;   /* Nome da Memoria Partilhada */
    Organizacao * organizacao = NULL;
    /* Abre memoria partilhada */
    organizacao = criaMemoriaPartilhada(nome, sizeof(Organizacao)*NUM_CARROS);

    for (int i = 0; i < NUM_CARROS; i++)
    {
        if (id == organizacao[i].id)
            return organizacao[i].pneus_disponiveis;
    }

    // nao encontrou da erro
    printf ("Pneus de Piloto nao foi encontrado");
    return -1;
}

char * buscaNomePiloto(int id)
{
    char nome[TAMANHO_NOME] = MEM_PARTILHADA;   /* Nome da Memoria Partilhada */
    Organizacao * organizacao = NULL;
    /* Abre memoria partilhada */
    organizacao = criaMemoriaPartilhada(nome, sizeof(Organizacao)*NUM_CARROS);

    for (int i = 0; i < NUM_CARROS; i++)
    {
        if (id == organizacao[i].id)
            return organizacao[i].nome_piloto;
    }

    // nao encontrou da erro
    printf ("Piloto nao foi encontrado");
    return NULL;
}

char * buscaEquipa(int id)
{
    char nome[TAMANHO_NOME] = MEM_PARTILHADA;   /* Nome da Memoria Partilhada */
    Organizacao * organizacao = NULL;
    /* Abre memoria partilhada */
    organizacao = criaMemoriaPartilhada(nome, sizeof(Organizacao)*NUM_CARROS);

    for (int i = 0; i < NUM_CARROS; i++)
    {
        if (id == organizacao[i].id)
            return organizacao[i].nome_equipa;
    }

    // nao encontrou da erro
    printf ("Equipa nao foi encontrada");
    return NULL;
}


int compara(const void *a, const void *b) {

    //Converte os ponteiros void para pointeiros nas struct StatusCarro
    const struct StatusCarro *structA = (const struct StatusCarro*)a;
    const struct StatusCarro *structB = (const struct StatusCarro*)b;


    if (structA->tempo < structB->tempo)
        return -1;
    if (structA->tempo > structB->tempo)
        return 1;

    return 0; // os valores são iguais
}


void imprimeResultados(Status tab[])
{
    // Chamada no codigo da função que ordena array com estrutura Carro
    qsort(tab, NUM_CARROS, sizeof(Status), compara);

    printf ("Posicao\tId\tNome\tEquipa\tVolta\tTempo\n");

    for (int t = 0; t < NUM_CARROS; t++)
    {
        printf ("%d\t%d\t%s\t%s\t%d\t%.3f\n",
                t+1,
                tab[t].id,
                buscaNomePiloto(tab[t].id),
                buscaEquipa(tab[t].id),
                tab[t].volta,
                tab[t].tempo);
    }
}

void escreveTC(int fd, const char *mensagem) {
    //printf("escreveTC\n");
    ssize_t bytes_written = write(fd, mensagem, strlen(mensagem));
    if (bytes_written == -1) {
        perror("Erro ao escrever no fifo");
        close(fd);
        exit(EXIT_FAILURE);
    }
}

void tiraUmCentilitro(double * deposito)
{
// a variavel deposito é disponivel por memoria partilhada
    double decremento = *deposito;
    decremento = decremento - 0.01;
    *deposito = decremento;
}

void * lerTelemetria(void * carro)
{

    int fd = 0; 						/* Identificador do FIFO */
    char mensagem[COMPRIMENTO_MENSAGEM + 1]; 	/* Mensagem lida do FIFO */
    char nome[TAMANHO_NOME] = MEM_PARTILHADA;   /* Nome da Memoria Partilhada */
    int tamanho_msg = 0;				/* Tamanho em bytes da mensagem */
    int fd2 = 0;
    Status *car_stat = (Status* ) carro;
    sem_t *semaphore;
    sem_t *semaphoreStart;
    char semaphore_name[TAMANHO_NOME];

    int hasTransmittedBoxMessage=0;

    sprintf(semaphore_name, "/Equipa%d.sem", car_stat->id+1);
    semaphore = sem_open(semaphore_name,0);

    double *deposito = criaMemoriaPartilhada("combustivel.shm", sizeof(double));

    sem_t *reabastecimento_semaphore = sem_open("/Reabastecimento.sem", 0);


    /* Cria o nome da FIFO "FIFO" + numero do carro + ".out." */
    /* Mensagens serão transmitidas neste pipe */
    sprintf (nome, "CARRO%d.out", car_stat->id+1);

    /* Cria FIFO  */
    if (mkfifo(nome, USER_LEVEL_PERMISSIONS) == TEM_ERRO)
    {
        printf("Erro ao criar FIFO %s\n",nome);
        pthread_exit(NULL);
    }


    /* Abre FIFO e associa a um identificar "fd", permissoes so escrita */
    /* Mensagens serão recebidas neste pipe */
    fd2 = open(nome, O_WRONLY);

    if(fd2 == TEM_ERRO)
    {
        printf("Erro no OPEN de %s \n",nome);
        pthread_exit(NULL);
    }


    /* Cria o nome da FIFO "FIFO" + numero do carro + ".in." */
    sprintf (nome, "CARRO%d.in", car_stat->id+1);

    /* Cria FIFO  */
    if (mkfifo(nome, USER_LEVEL_PERMISSIONS) == TEM_ERRO)
    {
        printf("Erro ao criar FIFO %s\n",nome);
        pthread_exit(NULL);
    }



    /* Abre FIFO e associa a um identificar "fd", permissoes so leitura */
    fd = open(nome, O_RDONLY);

    if(fd == TEM_ERRO)
    {
        printf("Erro no OPEN de %s \n",nome);
        pthread_exit(NULL);
    }

    semaphoreStart = sem_open("/InicioCorrida.sem",0);
    sem_wait(semaphoreStart);

    while (car_stat->volta != 10)
    {
        /* Ler mensagem da fifo */
        tamanho_msg = read(fd, mensagem, COMPRIMENTO_MENSAGEM);


        if (tamanho_msg == -1) {
            perror("Erro a ler do FIFO");
            close(fd);
            pthread_exit(NULL);
        }

        mensagem[tamanho_msg] = '\0';
        int ultimavolta=0;
        /* Imprime mensagem no ecra */
        if (tamanho_msg != 0)
        {
            /* Retira palavra e numero da mensagem */
            sscanf(mensagem, "| %hd | %d | %d | %f | %d | %d | %s |",
                   &car_stat->id, &car_stat->volta, &car_stat->setor,
                   &car_stat->tempo, &car_stat->combustivel, &car_stat->pneus, car_stat->tipo_pneu);
            if(car_stat->volta>ultimavolta){
                if(hasTransmittedBoxMessage){
                    sem_post(semaphore);
                    hasTransmittedBoxMessage=0;
                }
            }

            //printf("Msg: %s\n", mensagem);
            if ((car_stat->pneus < 10) || (car_stat->combustivel < 10))
            {
                if(buscaPneusDisponiveis(car_stat->id) < 4)
                {
                    printf ("Carro %d nao tem pneus disponiveis\n", car_stat->id);
                }
                else
                {
                    if(hasTransmittedBoxMessage){
                        sem_post(semaphore);
                    }
                    sem_wait(semaphore);
                    escreveTC(fd2, "BOX");
                    hasTransmittedBoxMessage=1;
                    ultimavolta=car_stat->volta;
                    int refillamount = (100-car_stat->combustivel)*100;

                    for(int i=0;i<refillamount;i++){
                        sem_wait(reabastecimento_semaphore);
                        tiraUmCentilitro(deposito);
                        sem_post(reabastecimento_semaphore);
                    }
                    car_stat->combustivel=100;


                }
            }
        }
    }

    printf("Corrida terminou para carro numero %d\n", car_stat->id);

    /* Fecha FIFOs */
    close(fd2);
    close(fd);

    /* Apaga as FIFOS */
    sprintf (nome, "CARRO%d.out", car_stat->id+1);
    unlink(nome);
    sprintf (nome, "CARRO%d.in ", car_stat->id+1);
    unlink(nome);
    pthread_exit(NULL);
}


void initTabela (Status tab[])
{
    for (int t = 0; t < NUM_CARROS; t++)
    {
        tab[t].id = t;
        tab[t].volta = 0;
        tab[t].setor = 0;
        tab[t].tempo = 0.0;
        tab[t].combustivel = 0;
        tab[t].pneus = 0;
        memset(tab[t].tipo_pneu, 0, 5);
    }
}

void * threadMaster(void * threads_arg) {
    char nome[TAMANHO_NOME];
    pthread_t* threads = (pthread_t*) threads_arg;
    char mensagem[COMPRIMENTO_MENSAGEM_MASTER];
    int fd;
    /* Cria FIFO  */
    if (mkfifo(MASTER_FIFO, USER_LEVEL_PERMISSIONS) == TEM_ERRO)
    {
        printf("Erro ao criar FIFO %s\n",MASTER_FIFO);
        pthread_exit(NULL);
    }

    /* Abre FIFO e associa a um identificar "fd", permissoes so leitura */
    fd = open(MASTER_FIFO, O_RDONLY);

    if (fd == -1) {
        perror("Error opening fifo master");
    }

    sem_t *semaphore;
    char semaphore_name[TAMANHO_NOME];


    semaphore = sem_open("/InicioCorrida.sem",0);
    if (semaphore == SEM_FAILED) {
        perror("Erro ao abrir o semáforo /InicioCorrida.sem");
        pthread_exit(NULL);
    }


    while(1) {
        int tamanho_msg = read(fd, mensagem, COMPRIMENTO_MENSAGEM_MASTER);
        if (tamanho_msg == -1) {
            perror("Erro a ler do FIFO");
            close(fd);
            pthread_exit(NULL);
        }

        mensagem[tamanho_msg] = '\0';

        if (strcmp(mensagem, "Partida!!") == 0) {
            printf("A corrida foi iniciada.\n");
            for (int i = 0; i < 9; i++) {
                sem_post(semaphore);
            }
        }else if (tamanho_msg != 0) {
            int car_id = 0;
            sscanf(mensagem, "despiste%d", &car_id);
            printf("Carro %d despistou-se\n", car_id);
            pthread_cancel(threads[car_id-1]);

            /* Apaga as FIFOS */
            sprintf (nome, "CARRO%d.out", car_id);
            unlink(nome);
            sprintf (nome, "CARRO%d.in ", car_id);
            unlink(nome);
        }
    }
}


void terminaCorridaAbruptamente() {
    char nome[TAMANHO_NOME];
    char semaphore_name[TAMANHO_NOME];
    double *deposito = NULL;
    Organizacao * organizacao = NULL;

    printf("Fim da corrida por tempo esgotado\n");


    deposito = criaMemoriaPartilhada("combustivel.shm", sizeof(double));
    organizacao = criaMemoriaPartilhada(MEM_PARTILHADA, sizeof(Organizacao)*NUM_CARROS);

    for (int i=0; i < NUM_CARROS; i++) {
        /* Apaga as FIFOS */
        sprintf (nome, "CARRO%d.out", i+1);
        unlink(nome);
        sprintf (nome, "CARRO%d.in", i+1);
        unlink(nome);
    }

    for (int i = 0; i < 9; i++) {
        sprintf(semaphore_name, "/Equipa%d.sem", i + 1);
        sem_unlink(semaphore_name);
    }
    sem_unlink("/InicioCorrida.sem");
    sem_unlink("/Reabastecimento.sem");
    apagaMemoriaPartilhada("combustivel.shm",sizeof(double),deposito);
    apagaMemoriaPartilhada(MEM_PARTILHADA,sizeof(Organizacao)*NUM_CARROS,organizacao);
    unlink(MASTER_FIFO);
    exit(0);
}

void bandeiraAmarelaLevantada(){
    printf("Bandeira amarela levantada\n");
}

void bandeiraAmarelaRecolhida(){
    printf("Bandeira amarela recolhida\n");
}

int main(int argc, char **argv)
{
    pthread_t threads[NUM_CARROS]; /*id threads para cada carro*/
    Status tabela[NUM_CARROS]; /* Tabela com status dos carros */
    char comando[COMPRIMENTO_MENSAGEM]; /* Comando para ativar o cliente*/
    // buffer[1000];
    int rc = 0;
    double *deposito = NULL;
    Organizacao * organizacao = NULL;

    deposito = criaMemoriaPartilhada("combustivel.shm", sizeof(double));
    organizacao = criaMemoriaPartilhada(MEM_PARTILHADA, sizeof(Organizacao)*NUM_CARROS);

    setvbuf(stdout, NULL, _IOLBF, 1024);

    if (argc >= 2)
    {
        sprintf (comando, "bash testscript.sh %d %s 2>&1", getpid(), argv[1]);
        FILE *pipe = popen(comando, "r");
        if (pipe == NULL)
        {
            printf("Erro no comando a chamar testscript \n");
            perror ("Erro :");
        }
    }
    else
    {
        printf ("Numero de argumentos invalido\n");
        exit(EXIT_SUCCESS);
    }

    signal(SIGUSR1, bandeiraAmarelaLevantada);
    signal(SIGUSR2, bandeiraAmarelaRecolhida);

    sem_t *semaphores[9];
    char semaphore_name[TAMANHO_NOME];
    sem_t *semaphore;
    sem_t *reabastecimento_semaphore;


    for (int i = 0; i < 9; i++) {
        sprintf(semaphore_name, "/Equipa%d.sem", i + 1);
        sem_unlink(semaphore_name);
    }
    sem_unlink("/InicioCorrida.sem");
    sem_unlink("/Reabastecimento.sem");


    for (int i = 0; i < 9; i++) {
        sprintf(semaphore_name, "/Equipa%d.sem", i + 1);
        semaphores[i] = sem_open(semaphore_name, O_CREAT, 0666, 1);
        if (semaphores[i] == SEM_FAILED) {
            perror("Erro ao criar o semáforo");
            exit(EXIT_FAILURE);
        }
    }
    semaphore = sem_open("/InicioCorrida.sem", O_CREAT, 0666, 0);
    reabastecimento_semaphore = sem_open("/Reabastecimento.sem", O_CREAT, 0666, 1);
    if (semaphore == SEM_FAILED) {
        perror("Erro ao criar o semáforo");
        exit(EXIT_FAILURE);
    }


    /** Lançando as threads */
    for (int t = 0; t < NUM_CARROS; t++)
    {
        initTabela(tabela);
        rc = pthread_create(&threads[t], NULL, lerTelemetria, (void *) &tabela[t]);
        if (rc)
        {
            fprintf(stderr, "Erro a crear thread %d; erro %d\n", t, rc);
            return 1;
        }
    }

    pthread_t master;
    rc = pthread_create(&master, NULL, threadMaster, (void*) threads);

    signal(SIGALRM, terminaCorridaAbruptamente);
    alarm(15);


    for (int t = 0; t < NUM_CARROS; t++)
    {
        rc = pthread_join(threads[t], NULL);
        if (rc)
        {
            fprintf(stderr, "Erro no join da thread %d; erro %d\n", t, rc);
            return 1;
        }
    }

    imprimeResultados(tabela);
    printf("O deposito de reserva da corrida e %.2f litros.\n",*deposito);

    for (int i = 0; i < 9; i++) {
        sem_close(semaphores[i]);
        sprintf(semaphore_name, "/Equipa%d.sem", i + 1);
        sem_unlink(semaphore_name);
    }

    pthread_cancel(master);
    unlink(MASTER_FIFO);
    sem_unlink("/InicioCorrida.sem");
    sem_unlink("/Reabastecimento.sem");
    apagaMemoriaPartilhada("combustivel.shm",sizeof(double),deposito);
    apagaMemoriaPartilhada(MEM_PARTILHADA,sizeof(Organizacao)*NUM_CARROS,organizacao);

    /* Espera 1 segundo para garantir que client tambem termina */
    sleep(1);

}
