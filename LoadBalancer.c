#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>

/* Valeur à entrer */

#define NB_MACHINE_MAX      5    // Nombre maximum possible de machines dans notre réseau P2P
#define NB_SERVEUR_MAX      1    // Nombre maximum de serveur par machine
#define PROCESS_SIZE        50
#define MAX_POURCENT        70
#define MIN_POURCENT        30

/* Structure d'un processus */


struct process{
    pid_t pid; 	                // Valeur du pid du processus   
	int gpid;	                // Identifiant global unique sur le réseau
	char *cmd;                  // Nom de la commande
}process[PROCESS_SIZE];

/* TAG */

#define TAG_TEST            0   // juste utiliser pour faire des tests
#define TAG_GSTART          1   // msg qui indique de faire un gstart
#define TAG_GSTART_CMD      2   // msg qui comporte les arguments de gstart
#define TAG_GPS             3   // msg qui indique de faire un gps
#define TAG_GKILL           4   // msg qui indique de faire un gkill
#define TAG_GKILL_GPID      5   // msg qui indique de retirer un certain gpid de sa matrice de processus
#define TAG_CHARGE          6   // msg pour la mise à jour de la charge 
#define TAG_GPID            7   // msg qui comporte le gpid que l'on doit ajouter à sa matrice de processus
#define TAG_RECHERCHE_GPID  8   // msg qui indique que l'on cherche la machine qui comporte un certain gpid
#define TAG_INSERTION       9   // msg qui porte l'identifiant de la machine qui s'insère dans le réseau
#define TAG_TRANSFERT       10  // msg qui porte le processus à transmettre à une autre machine
#define TAG_LESS            11  // msg qui demande à la machine la moins chargé de ce retirer du réseau
#define TAG_END             12  // msg qui indique au processus de ce terminer
#define TAG_PRESENT         13  // msg qui demande à un processus s'il est présent dans le réseau

/* Variables locales*/

int cpt_gpid = 1;                                           // Compteur global du gpid
float* tab_charge;                                          // Tableau des charges de l'ensemble des serveurs participants au réseau P2P
                                                            // indice de chaque case correspond au rang (identifiant) de la machine
float charge_globale;                                       // Moyenne des charges  
int* tab_participe;                                         // Tableau de booléen qui indique si le serveur (rank) est actif dans le réseau
int machines[NB_MACHINE_MAX*NB_SERVEUR_MAX][PROCESS_SIZE];  // Matrice de processus, qui stocke le lieu où chaque processus est stocké

/* Variables MPI */

int nb_proc;                                    // Nombre de serveurs dans le réseau
int rank;                                       // Identifiant du serveur dans le serveur P2P
MPI_Status status;                              // Structure permettant de récupérer le TAG du message reçu ainsi que son émetteur
char hostname[MPI_MAX_PROCESSOR_NAME];          // Nom de la machine sur lequel tourne le serveur
int length_hostname;                            // Taille du nom de la machine


void notifyCharge();
float CalculCharge();
void gkill(int signal, int pid, int gpid, int p);

/***************************************************************************************************
                            Fonctions d'initialisation et de terminaison
***************************************************************************************************/

/**
Fonction qui initialise MPI et les variables locales
*/

/**
 * @brief Fonction qui initialise MPI et les variables locales
 * 
 * @param argc      nombre de paramètres
 * @param argv      arguments
 */

void Init(int argc, char* argv[]){

    // Initialisation MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nb_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
    // On vérifie que le programme est lancé avec le bon nombre de processus maximum
    if (nb_proc < 2) {
        printf("Nombre de processus lancé insuffisant !\n");
        printf("Il faut lancer le programme avec au moins 2 processus.\n");
        MPI_Finalize();
        exit(2);
    }

    // Initialisation des variables
    MPI_Get_processor_name(hostname,&length_hostname);          // On récupère le nom de la machine
    tab_charge = (float *) malloc(nb_proc * sizeof(float));     // On alloue de la mémoire au tableau des charges
    tab_participe = (int *) malloc(nb_proc * sizeof(int));
    
    // Instancie la table des participants
    for(int i = 0; i < nb_proc; i++){
        tab_participe[i] = 1; 
    }


    notifyCharge();
}


/**
 * @brief Fonction qui gère la terminaison MPI et libère la mémoire alloué.
 * 
 */

void Final(){
    // Finalise MPI
    MPI_Finalize();

    // Libère l'espace mémoire alloué pour le programme
    free(tab_charge);
    free(tab_participe);
}

/***************************************************************************************************
                                Fonctions de gestions des machines
***************************************************************************************************/

/*******************************************INSERTION**********************************************/

/**
 * @brief Ajoute un participant au réseau
 * 
 */

void AddMachine(){

    // Parcours la table de participants
    for(int i = 1; i < nb_proc; i++){

        // Au premier non participant trouvé
        if(tab_participe[i] == 0){
            
            // Il devient participant
            tab_participe[i] = 1;
            
            // On prévient tout le monde 
            for(int j = 1; j < nb_proc; j++){
                // Sauf soi-même
                if(i != j)// On envoie l'id de celui qui va participer au réseau
                    MPI_Send(&i, 1, MPI_INT, j, TAG_INSERTION, MPI_COMM_WORLD);
            }
            break;
        }
    }
}


/**
 * @brief  transfert les taches d'une machine vers une autre qui particpe dans le réseau
 * 
 * @param id_machine        identifiant de la machine destinataire
 * @param more_or_less      1 si la machine est en surcharge, elle transfert qu'une tâche
 *                          0 si la machine est en souscharge, elle transfert toutes ses tâches
 */

void transfert_tache(int id_machine, int more_or_less){
    char *tab[2];
    int taille;
    
    // On parcours la table des processus lancé sur la machine
    for(int i=0; i < PROCESS_SIZE; i++){
        // Si une tâche est non nulle
        if(process[i].gpid != 0){
            // On récupère son gpid
            sprintf(tab[0], "%d", process[i].gpid);
            // On récupère le nom de la commande
            tab[1] = strdup(process[i].cmd);
            // transfert de la tache vers id_machine
            for(int k = 0; k < 2; k++){
                taille = strlen(tab[k]) + 1;
                MPI_Send(tab[k], taille, MPI_CHAR, id_machine, TAG_TRANSFERT, MPI_COMM_WORLD);
            }
            /*
            Envoie un gkill à soi même pour retirer le processus de sa table de processus
            Le gkill va ensuite informer tous les participants du réseau de retirer ce processus
            de leur table machines
            */
            gkill(9, process[i].pid, process[i].gpid, i);
            free(tab[1]);
            
            // Si c'est une surcharge, on s'arrête là, sinon on réitère jusqu'à ce qu'il n'y ai plus de processus dans la table
            if(more_or_less == 1)
                break;
        }
    }

}


/**
 * @brief surcharge - detection de la surcharge 
 * 
 * @return int      retourne 1 en cas de sucharge, sinon 0
 */

int surcharge(){
    // Si participant
    if(tab_participe[rank] == 1){

        // On calcul la charge globale moyenne
        charge_globale = CalculCharge();
        
        // Si on est en surcharge par rapport à la charge globale du réseau
        // On devrait aussi rajouter >= MAX_POURCENT*Nombre de coeur de la machine  
        if(tab_charge[rank] >= MAX_POURCENT*charge_globale){
            printf("%s JE SUIS EN SURCHARGE ET J AI POUR RANK %d \n",hostname,rank);
            
            float min = -1;
            int cpt = 0;

            // Parcours la table charge pour récupéré l'ensemble des id qui ont tag_charge = min
            for(int i = 1; i < nb_proc; i++){
                // Si un serveur participe et sa charge est inférieur au min (init à -1)
                if(tab_participe[i] && (tab_charge[i] < min)){
                    // tab_charge devient le nouveau min et le cpt est donc a 1
                    min = tab_charge[i];
                    cpt = 1;
                }else{
                    // si participe et égale à min alors on a plusieurs valeurs avec la meme charge min
                    // pour le cas où l'on a plusieurs serveurs par machine
                    if(tab_participe[i] && (tab_charge[i] == min)){
                        cpt++;
                    }
                }
            }
            int i = 0;  // indice du tableau des identifiants des charges min du réseau 
            int j = 1;  // indice pour parcourir la ta table des participants

            // tableau qui a la taille du nbr d'identifiants a avoir la charge min
            int tab[cpt];
            
            // Tant que le tableau n'est pas remplit et que j < nbr de serveur maximum du réseau
            while(i < cpt && j < nb_proc){ 
                // On récupère les id de tous les serveurs avec une charge egale a la charge min
                if(tab_participe[j] == 1 && tab_charge[j] == min){
                    tab[i] = j;
                    i++;
                }
                j++;
            }
            // ensuite on regarde si notre rang est égale a l'un des id qui a la charge min
            for(int i = 0; i < cpt; i++){
                // si c'est le cas on ajoute une machine pcq ca veut dire que
                // si on est l'une des machines a avoir une charge min ET
                // qu'on est surcharge alors peut importe a qui on donne une
                // tache, la machine cible sera en surcharge vu que l'on est l'un des min
                if(tab[i] == rank){
                    AddMachine();
                    return 1;
                }
            }
            // transfert de tache que si tu n'as pas ajouter de machine
            // car ca veut dire qu'il existe au moins une machine qui n'est pas
            // en surcharge donc on peut lui envoyer des taches
            transfert_tache(tab[0], 1);
            
        } // else la machine n'est pas en surcharge 
    }
    return 0;
}


/*******************************************RETRAIT***********************************************/

/**
 * @brief souscharge - détection de la sous-charge
 * 
 */

void souscharge(){
    float min = -1;
    int id_cible;
    int cpt = 0;
    if(tab_participe[rank] == 1){ // participant
        
        // On calcul la charge globale moyenne
        charge_globale = CalculCharge();  
         
        // Si on est en souscharge par rapport à la charge globale du réseau
        if(tab_charge[rank] <= MIN_POURCENT*charge_globale){ 
            printf("Machine %s en souscharge\n", hostname);

            // On cherche s'il y a au moins 2 participants dans le réseau
            for(int i = 1; i < nb_proc; i++){
                if(tab_participe[i] == 1){
                    cpt++;
                }

                if(cpt == 2)
                    break;
            }

            if(cpt < 2){
                printf("Pas assez de machine connecté pour en retirer.\n");
                return;
            }

            // On parcours la table des processus
            for(int i = 1; i < nb_proc; i++){

                // Si le processus est participant et qu'il est en sous charge
                if((tab_participe[i] == 1) && (tab_charge[i]<= MIN_POURCENT*charge_globale)){  
                    // ! cette vérification est importante car si jamais on a plusieurs machines en souscharge 
                    // ! on doit enlever qu'UNE seule machine
                    // ! donc on enlève la première qu'on trouve dans le tableau des charges globales du réseau
                    // ! on évite ainsi un interblocage
                    if(i == rank){ // je suis la première machine en sous charge
                        printf("Je suis en souscharge %s car %d=%d\n", hostname, i, rank);
                        // Donc je me retire du réseau et prévient les autres pour qu'elles ne m'envoient plus de messages
                        tab_participe[rank] = 0;
                        printf("%s JE ME RETIRE DU RESEAU!!!!!!!!!!!!!!!!\n",hostname);
                        //envoie un msg à tout le monde pour leur prévenir que je ne participe plus (c'est dommage)
                        for(int id = 1; id < nb_proc; id++){
                            if(id != rank){
                                MPI_Send(&rank, 1, MPI_INT, id, TAG_LESS, MPI_COMM_WORLD);
                            }
                        }
                        // trouver la première machine qui est active (sans se compter !!!)
                        // il en existe au moins une, cpt >=2
                        for(int j = 1; j < nb_proc; j++){
                            if((j != rank) && tab_participe[j] == 1){
                                id_cible = j;
                                min = tab_charge[j];
                                break;
                            }
                        }
                        // trouver la machine la moins chargée (sans se compter !!!)
                        for(int k = id_cible + 1; k < nb_proc; k++){
                            if(((k != rank) && (tab_participe[k] == 1)) && (tab_charge[k] < min)){
                                min = tab_charge[k];
                                id_cible = k;
                            }
                        }
                        // j'envoi mes taches à la machine cible avec un tag TAG_TRANSFERT
                        transfert_tache(id_cible, 0);
                    }
                    /* sinon
                    *    ce n'est pas moi qui est en premier mais quelqu'un d'autre
                    *    je ne fais rien et c'est la machine concernant qui le fera
                    */
                    break;
                }
            }

            
        } // else rien car pas en sous charge

    } // else non participant donc ne peux pas detecter de sous charge
}

/***************************************************************************************************
                                Fonctions de gestions des charges
***************************************************************************************************/

/**
 * @brief getCharge - récupère la charge calculé sur une minute de la machine 
 *                    à partir du fichier "/proc/loadavg"
 * 
 * @return float      retourne cette charge
 */
 
float getCharge(){
    float charge;
    FILE* fp = fopen("/proc/loadavg", "r");
    if(!fp) {
        perror("File opening failed");
        exit(0);
    }
    char buff[128];
    fgets(buff,128,fp);
    sscanf(buff,"%f",&charge);
    fclose(fp);
    return charge;
}

/**
 * @brief notifyCharge - permet de mettre au courant les autres machines de
 *                       la charge de la machine idMachine
 * 
 * @return * void 
 */
 
void notifyCharge(){
    tab_charge[rank] = getCharge();
    printf("%d a pour charge %2f\n", rank, tab_charge[rank]);
    
    for(int i = 1; i < nb_proc; i++){
        if((i != rank) && (tab_participe[i])){
            MPI_Send(&tab_charge[rank], 1, MPI_FLOAT, i, TAG_CHARGE, MPI_COMM_WORLD);
        }
    }
}

/**
 * @brief CalculCharge - calcule la moyenne des charges des machines
 *                       participant dans le réseau
 * 
 * @return float      retourne la moyenne des charges
 */

float CalculCharge(){
    float tmp_global = 0.0;
    int nbr_machine = 0;
    for(int i = 1; i < nb_proc; i++){
        if(tab_participe[i] == 1){
            tmp_global += tab_charge[i];
            nbr_machine += tab_participe[i];
        }
    }
    // Divise par le nombre de machine participantes
    tmp_global /= nbr_machine*1.0;
    return tmp_global;
}

/**
 * @brief getIdMachineMoinsCharge - recherche de la machine la moins chargée dans le réseau
 *                                  (la machine doit être participante)
 * 
 * @return int     retourne l'identifiant de la machine la moins chargée
 */


int getIdMachineMoinsCharge(){
    int i=0;

    // on cherche le premier identifiant de machine participant
    do{ 
        i++;
    }while((tab_participe[i] == 0) && i < nb_proc);

    int id = i;
    float min = tab_charge[i];
    
    // Parcours de la table des participants
    for(int i = id + 1; i < nb_proc; i++){
        // Si une machine participe et qu'elle a une charge inférieur à min
        if(tab_participe[i] && (tab_charge[i] < min)){
            min = tab_charge[i];
            id = i;
        }
    }
    return id;
}


/**
 * @brief handler - redéfinition du traitement du signal SIGALRM 
 * 
 * @param num 
 */

void handler(int num) {
    printf("Il est temps d'envoyer la charge à toutes les machines \n");
    notifyCharge();
    //  souscharge();
    /*
    if(!surcharge()){   // si la machine n'est pas en surcharge
        souscharge();   // vérification si elle est en souscharge
    }
    */
    if(tab_participe[rank] == 1){
        signal(SIGALRM, &handler);
        alarm(15);
    }else{
        alarm(0);
    }

}

/***************************************************************************************************
                                            TRANSIT CMD
***************************************************************************************************/

/**
 * @brief getPID_gpid - recherche du PID correspond au GPID
 * 
 * @param gpid              identifiant global du processus
 * @param indice_process    indice de la case des information du processus gpid
 * @return int              retourne le PID
 */

int getPID_gpid(int gpid, int *indice_process){
    // Parcours la table des processus
    for(int p = 0; p < PROCESS_SIZE; p++){
        // Si on trouve le processus grâce à son gpid
        if(process[p].gpid == gpid){
            printf("%s possède le processus de gpid %d.\n",hostname, gpid);
            *indice_process = p;
            return process[p].pid;
        }
    }
    // Si on ne le retrouve pas
    printf("Le processus avec le gpid %d n'existe pas.\n",gpid);
    return 0;
}


/**
 * @brief recv_gkill - retire le GPID du tableau machines
 * 
 * @param rank      identifiant de la machine qui était en charge du GPID
 * @param gpid      identifiant global à retirer
 * @param p         
 */

void recv_gkill(int rank, int gpid, int p){
    machines[rank][p] = 0;  
}

/***************************************************************************************************
                                                CMD
***************************************************************************************************/

/**
 * @brief gstart - permet de créer un processus exécutant une commande donnée
 *                 en paramètre sur la machine la moins chargée du réseau
 * 
 * @param args      tableau d'arguments pour la commande args[0] à lancer
 * @param gpid      identifiant global unique sur le réseau
 * @param indice    indice du tableau process
 * @return * void 
 */


void gstart(char * args[], int gpid, int indice_process){
    int pid = fork();
   
    if(pid==0){
        int i = 0;
        /* Le processus fils exécute la commande args[0] */
        if(!execvp(args[0],args)){
            perror("gstart : execvp failed\n");
            exit(0);
        }
    }
    printf("%s crée le processus %s qui a pour pid %d et gpid %d.\n", hostname, args[0], pid, gpid);
  
    /* Le père enregistre les informations du fils :
    *  - identifiant du processus (locale à la machine)
    *  - identifiant globale du processus (globale au réseau)
    *  - la commande
    */ 
    (process + indice_process)->pid = pid;
    (process + indice_process)->gpid = gpid;
    (process + indice_process)->cmd = strdup(args[0]);
}

/**
 * @brief gps - affiche la liste de tous les processus lancés par la machine
 *              sur laquelle on execute la commande gps
 * 
 * @param option     1 si option -l (affichage en format long), sinon 0   
 */
 
void gps(int option){
    int p;
    int uid = getuid();
    //affichage du tableau process local de la machine
    if(option == 0){ // sans option
        // affiche tous les processus de sa table des processus
        for(p = 0; p < PROCESS_SIZE; p++){
            if(process[p].pid != 0){ // Les cases non instancié sont ignorées
                printf("%d\t%d\t%s\n", process[p].pid, process[p].gpid, process[p].cmd);
            }
        }
    }else{ // format long car option -l

        //(noms executable, machine, uid, éventuellement statistiques d’utilisation CPU, mémoire)
        for(p = 0; p < PROCESS_SIZE; p++){
            if(process[p].pid != 0){ // Les cases non instancié sont ignorées
                printf("%s\t%d\t%d\t%d\t%s\n",hostname, uid, process[p].pid, process[p].gpid,process[p].cmd);
            }
        }   
    }
}


/**
 * @brief gkill - permet d'envoyer un signal sig à un processus pid
 * 
 * @param sig      numéro du signal
 * @param pid      identifiant du processus
 * @return * void 
 */
 
void gkill(int signal, int pid, int gpid, int p){
    char kill[20];

    // Lance la fct systeme kill
    printf("je dois kill le pid %d\n",pid);
    sprintf(kill, "kill -%d %d",signal, pid);
    printf("kill = %s\n", kill);
    system(kill);

    // On retire le processus de sa table de processus
    (process + p)->pid = 0;
    (process + p)->gpid = 0;
    (process + p)->cmd = NULL;

    // On prépare le message d'envoye
    int gkill_gpid[2];
    gkill_gpid[0] = gpid;
    gkill_gpid[1] = p;

    // On envoie le gpid et son indice dans la table de chaque machine
    // pour que chaque participant le retire
    for(int i = 1; i < nb_proc; i++){
        if((i != rank) && (tab_participe[i])){  
            MPI_Send(gkill_gpid, 2, MPI_INT, i, TAG_GKILL_GPID, MPI_COMM_WORLD);
        }
    } 

}

/***************************************************************************************************
                                                TEST
***************************************************************************************************/

/**
 * @brief test_gstart - gestion de la fonction gstart avant de l'envoyer dans le réseau
 * 
 * @param opt_gstart 
 */



void test_gstart(int opt_gstart){
    char* commande[4]= {NULL, NULL, NULL, NULL};
    char path[50];
    char option[5];
    int nb_sleep;
    int size = 1;
    // Selon l'option choisi dans le menu
    switch (opt_gstart){
    case 1: // date
        commande[0] = "date";
        break;

    case 2: // ls
        printf("Vous avez choisis la commande \"ls\"\n");
        printf("Veuillez indiquer quel répertoire vous voulez lister : \n");
        printf("\tEntrez \"none\" pour le répertoire courant\n");
        printf("\tSinon entrez le path\n");
        scanf("%s", &path);
        printf("Voulez vous ajouter une option? Si oui veuillez taper l'option sinon entrez \"none\"\n");
        scanf("%s",&option);
        printf("path %s\n",path);
        printf("option %s\n",option);
        commande[0] = "ls";
        if(strcmp(path, "none") == 0){ // répertoire courant
            if(strcmp(option, "none") != 0){ // pas d'option
                commande[1] = option;
                size = 2;
            }
        }else{ // autre répertoire
            if(strcmp(option, "none") == 0){ // pas d'option
                commande[1] = path;
                size = 2;
            }else{ // avec option
                commande[1] = option;
                commande[2] = path;
                size = 3;
            }
        }
        break;

    case 3: // ps
        printf("Vous avez choisis la commande \"ps\"\n");
        printf("Voulez vous ajouter une option? Si oui veuillez taper l'option sinon entrez \"none\"\n");
        scanf("%s",&option);
        printf("\n");
        if(strcmp(option, "none") == 0){ // pas d'option
            commande[0] = "ps";
        }else{ // avec option
            commande[0] = "ps";
            commande[1] = option;
            size = 2;
        }
        break;
        
    case 4: // executable
        printf("Vous avez choisis de lancer un exécutable\n");
        printf("Veuillez entrer un nombre de secondes. Attention : Par défaut il sera à 2.\n");
        scanf("%d", &nb_sleep);
        if(nb_sleep < 2)    nb_sleep = 2;
        char tmp[5];
        sprintf(tmp,"%d",nb_sleep);
        commande[0] = "./test";
        commande[1] = tmp;
        size = 2;
        break;
        
    default:
        printf("no good to be here \n");
        break;
    }

    // Envoie la taille de la commande
    MPI_Send(&size,1, MPI_INT, 1, TAG_GSTART, MPI_COMM_WORLD);
    
    // Envoie chaque élément de la commande
    for(int i=0; i<size; i++){
        MPI_Send(commande[i], strlen(commande[i])+1, MPI_CHAR, 1, TAG_GSTART_CMD, MPI_COMM_WORLD); 
    }
}


/**
 * @brief test_gps - gestion de la fonction gps avant de l'envoyer dans le réseau
 * 
 */

void test_gps(){
    int option = 0;
    char y_or_n[2];
    
    //demande à l'utilisateur s'il veut faire gps ou gps -l
    do{
        printf("Voulez-vous un affichage en format long ? (y/n)\n");
        scanf("%s", &y_or_n);
        if(strcmp(y_or_n, "y") == 0){
            option = 1;
            break;
        }
        if(strcmp(y_or_n, "n") == 0){
            break;
        }
        printf("Nous n'avons pas compris votre commande.\n");
    }while(1);
    
    if(option == 0){ // gps
        printf("PID\tGPID\tCMD\n");
    }else{  // gps -l
        printf("HOST\t\tUID\tPID\tGPID\tCMD\tCPU\tMEM\n");
    }

    //Envoi un message en précisant le format d'affichage (option) à toutes les machines de type TAG_GPS
    // pour leur dire d'afficher les processus courant de leur machine
    for(int i = 1; i < nb_proc; i++){
        MPI_Send(&option, 1, MPI_INT, i, TAG_GPS, MPI_COMM_WORLD);
    }
    sleep(1);
}

/**
 * @brief test_gkill - gestion de la fonction gskill avant de l'envoyer dans le réseau
 * 
 */

void test_gkill(){
   int sig;
   int gpid;
   int id_machine;
   int tab_gkill[2];
   char y_or_n[2];
   
    printf("Connaissez-vous le GPID du processus à qui vous allez envoyer un signal ? (y/n)\n");
    scanf("%s", &y_or_n);
    if(strcmp(y_or_n, "n") == 0 ){
        printf("Vous devez passer par la commande \"gps\"\n");
        //sleep(2);
    }else{
        do{
            printf(" Liste des signaux:\n");
            //system("kill -l");
            printf(" 1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL       5) SIGTRAP\n");
            printf(" 6) SIGABRT      7) SIGBUS       8) SIGFPE       9) SIGKILL     10) SIGUSR1\n");
            printf(" 11) SIGSEGV     12) SIGUSR2     13) SIGPIPE     14) SIGALRM     15) SIGTERM\n");
            printf(" 16) SIGSTKFLT   17) SIGCHLD     18) SIGCONT     19) SIGSTOP     20) SIGTSTP\n");
            printf(" 21) SIGTTIN     22) SIGTTOU     23) SIGURG      24) SIGXCPU     25) SIGXFSZ\n");
            printf(" 26) SIGVTALRM   27) SIGPROF     28) SIGWINCH    29) SIGIO       30) SIGPWR\n");
            printf(" 31) SIGSYS      34) SIGRTMIN    35) SIGRTMIN+1  36) SIGRTMIN+2  37) SIGRTMIN+3\n");
            printf(" 38) SIGRTMIN+4  39) SIGRTMIN+5  40) SIGRTMIN+6  41) SIGRTMIN+7  42) SIGRTMIN+8\n");
            printf(" 43) SIGRTMIN+9  44) SIGRTMIN+10 45) SIGRTMIN+11 46) SIGRTMIN+12 47) SIGRTMIN+13\n");
            printf(" 48) SIGRTMIN+14 49) SIGRTMIN+15 50) SIGRTMAX-14 51) SIGRTMAX-13 52) SIGRTMAX-12\n");
            printf(" 53) SIGRTMAX-11 54) SIGRTMAX-10 55) SIGRTMAX-9  56) SIGRTMAX-8  57) SIGRTMAX-7\n");
            printf(" 58) SIGRTMAX-6  59) SIGRTMAX-5  60) SIGRTMAX-4  61) SIGRTMAX-3  62) SIGRTMAX-2\n");
            printf(" 63) SIGRTMAX-1  64) SIGRTMAX\n");
            printf("\nVeuillez entrer le numéro du signal: \n");
            scanf("%d", &sig);
            if(sig>0 && sig <= 64){ // vérification du numéro de signal entrer par l'utilisateur
                break;
            }
            printf("\nNous n'avons pas compris le numéro que vous avez indiquer. Veuillez réessayer\n");
        }while(1);
        printf("Veuillez entrer le GPID \n");
        scanf("%d", &gpid);
        
        // Tableau d'envoi contenant le numéro du signal ainsi que le gpid du processus auquel on veut envoyer un signal sig 
        tab_gkill[0] = sig;
        tab_gkill[1] = gpid;
        
        // envoyer la recherche a une machine participante car celle qui lance les test ne fait jamais de recv
        if(rank != nb_proc - 1){
            MPI_Send(tab_gkill, 2, MPI_INT, (rank+1)%nb_proc, TAG_RECHERCHE_GPID, MPI_COMM_WORLD);
        }else{
            MPI_Send(tab_gkill, 2, MPI_INT, 1, TAG_RECHERCHE_GPID, MPI_COMM_WORLD);
        }
    }
}

/**
 * @brief test_present - envoi un message à toutes les machines
 *                       A la reception de ce message, si la machine participe dans le réseau
 *                       elle devra faire un affichage (cf fonction receive TAG_PRESENT)
 *                       
 */

void test_present(){
    int k = 0;
    for(int i = 1; i < nb_proc; i++){
        MPI_Send(&k, 1, MPI_INT, i, TAG_PRESENT, MPI_COMM_WORLD);
    }
}
/***************************************************************************************************
                                               LANCER
***************************************************************************************************/

/**
 * @brief lancer_gstart - permet de générer un GPID unique, de notifier ce dernier à toutes les machines
 *                        et ensuite lance l'appel à la fonction gstart
 * 
 * @param argv         contient le nom de la commande [option] [arguments]
 */

void lancer_gstart(char **argv){
    int tab_gpid_indice[2];
    int indice_process;
    int gpid;
    
    // Génération du gpid
    gpid = rank * 1000 + cpt_gpid; //l'unicité du gpid est garantit grâce à la valeur rank
    cpt_gpid++;
    
    // recherche d'un emplacement disponible dans le tableau process
    for(int i = 0; i < PROCESS_SIZE; i++){
        if(process[i].pid == 0){
            indice_process = i; // enregistre l'indice de cet emplacement
            break;
        }
    }
    
    machines[rank][indice_process] = gpid; // enregistre le gpid dans le tableau machines
   
    // Tableau d'envoi contenant le gpid et l'indice de l'emplacement
    tab_gpid_indice[0] = gpid;
    tab_gpid_indice[1] = indice_process;
   

    // Envoi un message du GPID généré à toutes les machines participantes dans le réseau 
    for(int i = 1; i < nb_proc; i++){
        if((i!=rank) && (tab_participe[i] == 1)){
            MPI_Send(tab_gpid_indice, 2, MPI_INT, i, TAG_GPID, MPI_COMM_WORLD);  
        }
    }
    
    // Création d'un processus + exécution de la tache
    gstart(argv, gpid, indice_process);

}


/***************************************************************************************************
                                                RECV
***************************************************************************************************/

/**
 * @brief receive - réceptions et traitements des différents messages
 * 
 */
 

void receive() {
    int tab_gpid_indice[2]; //TAG_TRANSFERT, TAG_GPID
    int tab_gkill[2];       //TAG_GKILL, TAG_GKILL_GPID
    char* tab_transfert[2]; //TAG_TRANSFERT
    int indice_process;     //TAG_GKILL
    int size;               //TAG_GSTART, TAG_TRANSFERT
    int size_cmd;           //TAG_GSTART
    int option;             //TAG_GPS
    int source;             //TAG_GSTART, TAG_GPS, TAG_TRANSFERT, TAG_LESS
    //int gpid;
    int id_machine;         //TAG_GSTART, TAG_INSERTION, TAG_LESS, TAG_END
    int gkill_gpid[2];      //TAG_GKILL_GPID
    int ok;                 //TAG_TRANSFERT
    int k;                  //TAG_PRESENT
    int end = 0;            //TAG_END
    while(end == 0){

        //sleep(5);
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        switch (status.MPI_TAG){
            case TAG_CHARGE:
                // Récupère et enregistre la charge de la machine source
                MPI_Recv(&tab_charge[status.MPI_SOURCE], 1, MPI_FLOAT, status.MPI_SOURCE, TAG_CHARGE, MPI_COMM_WORLD, &status);
                break;

            case TAG_GSTART:
                source = status.MPI_SOURCE;
                // Réception du nombre d'élément qu'on va recevoir après cette réception pour la commande
                MPI_Recv(&size, 1, MPI_INT, source, TAG_GSTART,  MPI_COMM_WORLD, &status);
                char** commande = malloc(sizeof(char*)*(size+1));

                // Réception + enregistre dans la variable commande les informations de cette dernière
                for(int i = 0; i < size; i++){
                    MPI_Probe(source, TAG_GSTART_CMD, MPI_COMM_WORLD, &status);
                    MPI_Get_count(&status, MPI_CHAR, &size_cmd);
                    commande[i] = malloc(sizeof(char)*size_cmd);
                    MPI_Recv(commande[i], size_cmd, MPI_CHAR, source, TAG_GSTART_CMD,  MPI_COMM_WORLD, &status);
                }
                commande[size] = NULL;
                
                
                if(tab_participe[rank] == 0){ // si je ne participe plus
                    // J'envoi au suivant
                    MPI_Send(&size,1, MPI_INT,(rank+1)%nb_proc, TAG_GSTART,MPI_COMM_WORLD);
                    for(int i=0; i<size; i++){
                        if(rank != nb_proc - 1)
                            MPI_Send(commande[i], strlen(commande[i])+1, MPI_CHAR, (rank+1)%nb_proc, TAG_GSTART_CMD, MPI_COMM_WORLD); 
                        else
                            MPI_Send(commande[i], strlen(commande[i])+1, MPI_CHAR, 1, TAG_GSTART_CMD, MPI_COMM_WORLD); 
                    }
                }else{
                    // Sinon je suis participant
                    //Récupère la machine l'identifiant de la machine la moins chargé du réseau
                    id_machine = getIdMachineMoinsCharge();
                    if(id_machine == rank){ // si je suis la machine la moins chargée du réseau
                        lancer_gstart(commande);
                    }else { // je ne suis pas la machine la moins chargée
                            // c'est une autre machine du réseau soit id_machine
                            // Envoi les informations de la commande à id_machine
                        MPI_Send(&size,1, MPI_INT,id_machine, TAG_GSTART,MPI_COMM_WORLD);
                        for(int i=0; i<size; i++){
                            MPI_Send(commande[i], strlen(commande[i])+1, MPI_CHAR, id_machine, TAG_GSTART_CMD, MPI_COMM_WORLD); 
                        }
                    }
                }

                for(int i=0; i < size + 1; i++){ 
                	free(commande[i]);
                }
                free(commande);

                break;
            
            case TAG_GPID:
                // Réception du gpid généré par la machine source
                MPI_Recv(tab_gpid_indice, 2, MPI_INT, status.MPI_SOURCE, TAG_GPID, MPI_COMM_WORLD,&status); 
                machines[status.MPI_SOURCE][tab_gpid_indice[1]] = tab_gpid_indice[0];
                break;
            
            case TAG_GPS:
                source = status.MPI_SOURCE;
                // Réception du message GPS demandant de faire un affichage des processus courant dans ma machine
                MPI_Recv(&option, 1, MPI_INT, source, TAG_GPS, MPI_COMM_WORLD, &status);
                if(tab_participe[rank] == 1){ // si je suis participante 
                    gps(option);              // appel à gps pour afficher
                }
                break;

            case TAG_GKILL :
                // Reception d'un message demandant d'envoyer un signal à un processus
                // tab_gkill contient le numéro du signal et le gpid
                MPI_Recv(tab_gkill, 2, MPI_INT, status.MPI_SOURCE, TAG_GKILL, MPI_COMM_WORLD, &status);
                // Récupération du PID correspond au GPID
                // de plus dans indice_process on récupère l'emplacement des informations du processus pid
                int pid = getPID_gpid(tab_gkill[1],&indice_process); 
                if(pid != 0){ // envoi un signal tab_gkill[0] à pid
                    gkill(tab_gkill[0], pid, tab_gkill[1], indice_process);
                }      
                break;
        
            case TAG_GKILL_GPID :
                // Réception de l'information que GPID (contenu dans gkill_gpid) de la machine source n'est plus là
                MPI_Recv(gkill_gpid, 2, MPI_INT, status.MPI_SOURCE, TAG_GKILL_GPID, MPI_COMM_WORLD, &status);
                recv_gkill(status.MPI_SOURCE, gkill_gpid[0], gkill_gpid[1]);
                break;
            
            case TAG_RECHERCHE_GPID:
                // Recherche du responsable du GPID 
                MPI_Recv(tab_gkill, 2, MPI_INT, status.MPI_SOURCE, TAG_RECHERCHE_GPID, MPI_COMM_WORLD, &status);
                if(tab_participe[rank] == 0){ // Je ne suis pas participant donc j'envoi à quelqu'un d'autre
                    if(rank != nb_proc - 1)
                        MPI_Send(tab_gkill, 2, MPI_INT, (rank+1)%nb_proc, TAG_RECHERCHE_GPID, MPI_COMM_WORLD);
                    else
                        MPI_Send(tab_gkill, 2, MPI_INT, 1, TAG_RECHERCHE_GPID, MPI_COMM_WORLD);
                }else{ // Je suis participant
                    id_machine = -1;
                    // Parcours l'ensemble des machines du réseau
                    for(int i = 1; i < nb_proc; i++){
                        //if(tab_participe[i] == 1){
                            //Parcours l'ensemble des processus de la machine 
                            for(int j = 0; j < PROCESS_SIZE; j++){
                                if(machines[i][j] == tab_gkill[1]){
                                    id_machine = i;
                                    break;
                                }
                            }
                        //}
                        if(id_machine != -1){
                            break;
                        }
                    }
                    MPI_Send(tab_gkill, 2, MPI_INT, id_machine, TAG_GKILL, MPI_COMM_WORLD);

                    
                }
            case TAG_INSERTION :
                // Reçoit l'id de la machine qui va rentrer dans le réseau
                MPI_Recv(&id_machine, 1, MPI_INT, status.MPI_SOURCE, TAG_INSERTION, MPI_COMM_WORLD, &status);
                tab_participe[id_machine] = 1;
                // Si on est l'id de la machine
                if(id_machine == rank)
                    printf("%s JE M'INSERT DANS LE RESEAU!!!!!!!!!!!!\n",hostname);
                break;

            case TAG_TRANSFERT:
                // On récupère le gpid et cmd en char* dans tab transfert
                source = status.MPI_SOURCE;
    
                for(int k = 0; k < 2; k ++){
                    if(k == 1)
                        MPI_Probe(source, TAG_TRANSFERT, MPI_COMM_WORLD, &status);
                    MPI_Get_count(&status, MPI_CHAR, &size);
                    tab_transfert[k] = malloc(sizeof(char)*size);
                    MPI_Recv(tab_transfert[k], size, MPI_CHAR, source, TAG_TRANSFERT, MPI_COMM_WORLD, &status);
                }
                ok = 0; // indique s'il y a encore de la palce dans la table des proccesus
                
                //Parcours la table des processus
                for(int i = 0; i < PROCESS_SIZE; i++){

                    // premier process non nulle
                    if(process[i].gpid == 0) {
                        int pid = fork();
                        
                        if(pid == 0){ // fait rien
                            if(!execlp("ls","ls", NULL)){ // exec pour eviter que le fils ne finisse avant le père et fasse un finalize
                                perror("gstart : execvp failed\n");
                                exit(0);
                            }
                        } 
                        //(process + i)->gpid = atoi(tab_transfert[0]);
                        //(process + i)->cmd = strdup(tab_transfert[1]);
                        //(process + i)->pid = pid;
                        process[i].gpid = atoi(tab_transfert[0]);
                        process[i].cmd = strdup(tab_transfert[1]);
                        process[i].pid = pid;
                        ok = 1;

                        // prevenir l'existence du nouveau processus
                        machines[rank][i] = process[i].gpid;
                        tab_gpid_indice[0] = process[i].gpid;
                        tab_gpid_indice[1] = i;

                        // En informe tous les machines participantes
                        for(int j = 1; j < nb_proc; j++){
                            if((j!=rank) && (tab_participe[j] == 1)){
                                MPI_Send(tab_gpid_indice, 2, MPI_INT, j, TAG_GPID, MPI_COMM_WORLD); // envoie  du gpid généré  
                            }
                        }
                        break;
                    }            
                }

                // DANS LE CAS PAS DE PLACE FAUT DETRUIRE LE MSG ET PREVENIR TLM
                if(!ok){
                    // comme non ok, alors ca veut dire que le processus n'a pas été recréé
                    printf("%s : \n Recv - TAG_TRANSFERT : Impossible d'ajouter le processus dont le gpid est %s \n", hostname, tab_transfert[0]);
                }
                free(tab_transfert[0]);
                free(tab_transfert[1]);
                break;

            case TAG_LESS:
                // Réception de l'identifiant de la machine qui s'est retirée
                source = status.MPI_SOURCE;
                MPI_Recv(&id_machine, 1, MPI_INT , source, TAG_LESS, MPI_COMM_WORLD, &status);
                tab_participe[id_machine] = 0; // enregistre du retrait de la machine id_machine
                break;

            case TAG_END:
                // L'utilisateur a décider de quitter le menu
                // La machine doit arrêter 
                MPI_Recv(&id_machine, 1, MPI_INT, 0, TAG_END, MPI_COMM_WORLD, &status);
             //   alarm(0);
                end = 1;
                break;
            
            case TAG_PRESENT:
                // Réception d'un message de type TAG_PRESENT
                // Si je suis participante alors j'affiche pour indiquer ma présence dans le réseau
                MPI_Recv(&k, 1, MPI_INT, 0, TAG_PRESENT, MPI_COMM_WORLD, &status);
                if(tab_participe[rank] == 1){
                    printf("%s participe au réseau et à un serveur d'id %d\n",hostname,rank);
                }
                break;
                
            default:
                printf("%s  Erreur : Ne doit pas arriver ici, tag %d\n", hostname, status.MPI_TAG);
                break;
        }
    }
}

/***************************************************************************************************
                                                MENU
***************************************************************************************************/

/*
Fonction qui affiche le menu
*/

/**
 * @brief menu - Affichage du menu à l'utilisateur
 * 
 */

void menu() {
    int option;
    char opt_gstart[50];
    char quit[50];
    
    printf("Projet PSAR - Un répartiteur de charge pour des machines en réseau\n");
    menu :
        printf("\n~~~~ Commandes ~~~~\n");
        printf("0 : Explication des commandes\n");
        printf("1 : gstart\n");
        printf("2 : gps [-l]\n");
        printf("3 : gkill\n");
        printf("4 : afficher les machines connecté sur le réseau\n");
        printf("5 : Quitter le MENU\n");

        printf("Entrez une option\n");
        scanf("%d", &option);
        printf("option vaut %d\n", option);
        switch (option){
            case 0:
                printf("Explication des commandes : \n");
                printf("----------------------------------------------------------------------------------------------------------------------------------\n");
                printf("gstart : \n");
                printf("\tCrée un processus exécutant “ prog arguments ” sur la machine la moins chargée du réseau.\n");
                printf("\n\tprog :\n");
                printf("\t\t0 - date : Affiche la date et l'heure actuelle.\n");
                printf("\t\t1 - ls [arg] : Liste le contenu du répertoire selon les options préciser dans arg.\n");
                printf("\t\t2 - ps [-l] : Liste les processus en cours d'exécution dans la machine la moins chargée (format standard ou format long avec -l).\n");
                printf("\t\t3 - Tâche de X secondes.\n");
                printf("\n");
                printf("----------------------------------------------------------------------------------------------------------------------------------\n");
                printf("gps : \n");
                printf("\tSans l'option -l:\n");
                printf("\t\tAffiche la liste de tous les processus qui ont été lancés sur le réseau.\n");
                printf("\tAvec l'option -l:\n");
                printf("\t\tAffiche un format long (noms executable, machine, uid, CPU, mémoire).\n");
                printf("----------------------------------------------------------------------------------------------------------------------------------\n");
                printf("gkill -sig gpid : \n");
                printf("\tsig : identifiant d'un signal.\n");
                printf("\tgpid : identifiant global unique sur le réseau d'un processus.\n");
                printf("----------------------------------------------------------------------------------------------------------------------------------\n");

                printf("\nEntrez une touche pour retourner dans le menu principal.\n");
                scanf("%s", &quit);
                goto menu;
                break;
                
            case 1:
                do{
                    printf("Vous avez choisi la commande gstart.\n");
                    printf("Entrer le nom d'une commande ou d'un exécutable que vous voulez exécuter:\n");
                    printf("~~~~ Commandes ~~~~\n");
                    printf("1 - date\n");
                    printf("2 - ls [arg]\n");
                    printf("3 - ps [-l]\n");
                    printf("4 - Lancer une tâche de X seconde(s)\n");
                    printf("5 - Retour au menu\n");
                    printf("Veuillez entrer une valeur.\n");
                    scanf("%s",&opt_gstart);
                    int opt_gstart2 = atoi(opt_gstart);
                    if( opt_gstart2 == 5) {
                        goto menu;
                    }else if( opt_gstart2>= 1 && opt_gstart2< 5){
                        
                        test_gstart(opt_gstart2);
                        // sleep(2);
                    }else {
                        printf("Nous n'avons pas compris votre commande.\n");
                        //sleep(2);
                    }
                }while(1);
                break;

            case 2:
                printf("Vous avez choisi la commande gps.\n");
                test_gps();
                goto menu;
                break;
            case 3:
                printf("Vous avez choisi gkill\n");
                printf("Pour pouvoir faire gkill, vous devez connaitre les processus existants dans le réseau.\n");
                test_gkill();
                goto menu;
                break;

            case 4:
                printf("Les machines présentes sur le réseau sont :\n");
                test_present();
                goto menu;
                break;
            case 5:
                printf("Vous avez choisi de quitter le MENU\n");
                printf("Merci et Au revoir :) \n");
                for(int i = 1; i < nb_proc; i++){
                    MPI_Send(&rank, 1, MPI_INT, i, TAG_END, MPI_COMM_WORLD);
                }
                break;

            default:
                printf("Nous n'avons pas compris votre commande.\nRetour au menu principal.\n");
                goto menu;
                break;
        }
}

/***************************************************************************************************
                                                MAIN
***************************************************************************************************/

int main (int argc, char* argv[]) {  
    // Initialisation de notre programme
    Init(argc, argv);

    if(rank == 0){
        menu();
    }else{
        signal(SIGALRM, &handler);
        alarm(15);
        receive();
    }

    // Finalisation de notre programme
    Final();

    return 0;
}

/*
-   La machine ajouter doit copier l'état d'une autre machine pour etre a jour
-   fct equilibrage pour surcharge : quand ajoute une machine il faut équilibrer 
-   gps manque CPU et MEM
*/