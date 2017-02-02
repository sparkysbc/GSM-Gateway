/*
 * liballogsmat: An implementation of ALLO GSM cards
 *
 * Written by pawan
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "liballogsmat.h"
#include "gsm_internal.h"

queueADT QueueCreate(void);

int QueueIsEmpty(queueADT queue){

        if(queue->front)
                return 0; /* Queue is not empty*/
        else
                return 1; /* Queue is empty*/

}
int QueueEnter(struct allogsm_modul *gsm, sms_info_u sms_info)
{
	queueADT queue = gsm->sms_queue;
        queueNodeT *newNodeP;

        /* Allocate space for a node in the linked list. */

        newNodeP = (queueNodeT *)malloc(sizeof(queueNodeT));

        if (newNodeP == NULL) {
                gsm_error(gsm, "Insufficient memory for new queue sms_info.\n");
                return -1;
        }

        /* Place information in the node */

        newNodeP->sms_info = sms_info;
        newNodeP->next = NULL;
	gsm_message(gsm, "here at enqueue %d.. dest: %s, msg:%s\n",__LINE__ ,newNodeP->sms_info.txt_info.destination, newNodeP->sms_info.txt_info.message);
	
        /*Check Queue is present or not.. if not create it*/
        if (queue == NULL){
                gsm_message(gsm, "queue is empty\n");
//              queue = QueueCreate();
                return -2;
        }
        if (queue == NULL)
                gsm_message(gsm, "queue is still empty.. how\n");
        /*
        ** Link the sms_info into the right place in
        ** the linked list.
        **/

        if (queue->front == NULL) {  /* Queue is empty */
                queue->front = queue->rear = newNodeP;
		gsm_message(gsm, "here at enqueue %d..\n",__LINE__); //pawan print
		/*pawan sched here*/
        } else {
		gsm_message(gsm, "here at enqueue %d..\n",__LINE__); //pawan print
                queue->rear->next = newNodeP;
                queue->rear = newNodeP;
        }
        return 0;
}

/* This is a queue and it is FIFO, so we will always remove the first element */
//struct my_list* QueueDelete( struct my_list* s )
int QueueDelete(struct allogsm_modul *gsm)
{
	queueADT queue = gsm->sms_queue;
        queueNodeT *newNodeF, *newNodeP;
        newNodeF = NULL;
        newNodeP = NULL;
//	gsm->sms_info=&(gsm->last_sms);
        memset(gsm->sms_info, 0, sizeof (sms_info_u));

        if( NULL == queue ){
                gsm_message(gsm, "List is empty\n");
                return -1;
        } else if ( NULL == queue->front && NULL == queue->rear ){
                gsm_message(gsm, "Well, List is empty\n");
                return -1;
        } else if ( NULL == queue->front || NULL == queue->rear ){
                gsm_message(gsm, "There is something seriously wrong with your list\n");
                gsm_message(gsm, "One of the front/rear is empty while other is not \n");
                return -1;
        }
//gsm->sms_info = sms_info;

	gsm_message(gsm, "here after dequeue %d.. dest: %s, msg:%s\n",__LINE__ ,queue->front->sms_info.txt_info.destination, queue->front->sms_info.txt_info.message);
        newNodeF = queue->front ;
        newNodeP = newNodeF->next;
        memcpy(gsm->sms_info,&(queue->front->sms_info), sizeof(sms_info_u));
        free(newNodeF);
	gsm_message(gsm, "here after dequeue %d.. dest: %s, msg:%s\n",__LINE__ ,gsm->sms_info->txt_info.destination, gsm->sms_info->txt_info.message);
        queue->front = newNodeP;
        if( NULL == queue->front )  queue->rear = queue->front ;   /* The element rear was pointing to is free(), so we need an update */
        if (queue->front== NULL){
		/**/
                gsm_message(gsm, "after deletion queue front is null.. \n");
	}else{
		/*pawan sched here*/
                return 1;
	}
        return 0;
}

queueADT QueueCreate(void)
{
  queueADT queue;

  queue = (queueADT)malloc(sizeof(queueCDT));

  if (queue == NULL) {
	fprintf(stderr, "Insufficient memory for new queue.\n");
  }

  queue->front = queue->rear = NULL;

  return queue;
}

void QueueDestroy(queueADT queue)
{
  /*
 * * First remove each sms_info from the queue (each
 * * sms_info is in a dynamically-allocated node.)
 * */
        if (queue == NULL){
                fprintf(stderr, "Queue Doesnot exist, Cant Destroy\n");
                return;
        }

  //while ( queue->front )
  while (!QueueIsEmpty(queue)){
        fprintf(stderr, "In destroy, Deleting sms\n");
    QueueDelete(queue);
        }

  /*
 * * Reset the front and rear just in case someone
 * * tries to use them after the CDT is freed.
 * */
  queue->front = queue->rear = NULL;

  /*
 * * Now free the structure that holds information
 * * about the queue.
 * */
  free(queue);
}


