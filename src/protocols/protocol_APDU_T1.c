/*
 * Protocol APDU T1
 * Expose API for Application Protocol Data Unit in protocol T1
 */

#include "smartcard_config.h"

#include "sc_defs.h"
#include "slot_itf.h"
#include "EDC.h"

#include "protocols.h"

#include "sc_debug.h"

/************************************************************************************
 * Private defines
 ************************************************************************************/

#undef MAX
#define MAX(a,b)                    ((a) > (b) ? (a) : (b))

#undef MIN
#define MIN(a,b)                    ((a) < (b) ? (a) : (b))

#define END_TRANSACTION(__ERR__)    ret = __ERR__;\
                                    state = APDU_T1_end_of_transaction;\
                                    break;

#define HASMORE(__PCB__)            (((__PCB__) & (PCB_I_MORE))!=0x00)
#define ISSRESP(__PCB__)            (((__PCB__) & (PCB_S_RESPONSE))!=0x00)
#define ISSREQ(__PCB__)             (!ISSRESP((__PCB__)))
#define STYPE(__PCB__)              (((__PCB__) & 0x03))

#define READER                      0
#define CARD                        1


/************************************************************************************
 * Private variables
 ************************************************************************************/


/************************************************************************************
 * Private types
 ************************************************************************************/

typedef enum {
    APDU_T1_start_of_transaction,
    APDU_T1_transact,
    APDU_T1_process_I_block,
    APDU_T1_process_R_block,
    APDU_T1_process_S_block,
    APDU_T1_prepare_next_block,
    APDU_T1_resynch_request,
    APDU_T1_end_of_transaction,
    APDU_T1_exit
}APDU_T1_state;


/************************************************************************************
 * Private functions
 ************************************************************************************/

static sc_Status    build_I_block                       (   iso_params_t*   params,
                                                            uint8_t*        block,
                                                            uint32_t*       block_len,
                                                            uint8_t*        data_buffer,
                                                            uint32_t        data_len
                                                        )
{
    uint32_t len;
    uint16_t CRC;
    uint8_t PCB;

    PCB = PCB_I_BLOCK | (params->Nd << 6);

    len = MIN(data_len, params->IFSC);
    if(len < data_len ){
        PCB |= PCB_I_MORE;
    }

    block[NAD_IDX] = (params->DAD << 4) | params->SAD;
    block[PCB_IDX] = PCB;
    block[LEN_IDX] = len;
    *block_len = T1_PROLOGUE_SIZE;

    common_memcpy(block + T1_PROLOGUE_SIZE, data_buffer, len);
    *block_len += len;

    if(params->EDC == SC_EDC_LRC){
        block[*block_len] = EDC_LRC(block, *block_len);
        (*block_len)++;
    }else{
        CRC = EDC_CRC(block, *block_len);
        block[(*block_len)++] = CRC >> 8;
        block[(*block_len)++] = CRC & 0xFF;
    }
    return sc_Status_Success;
}

static sc_Status    build_S_block                       (   iso_params_t*   params,
                                                            uint8_t         PCB_options,
                                                            uint8_t*        block,
                                                            uint32_t*       block_len,
                                                            uint8_t*        optional_byte
                                                        )
{
    uint8_t     len;
    uint16_t    CRC;

    len = 3;

    block[NAD_IDX] = (params->DAD << 4) | params->SAD;
    block[PCB_IDX] = PCB_S_BLOCK | PCB_options;
    if(optional_byte){
        block[LEN_IDX] = 1;
        block[3] = *optional_byte;
        len ++;
    }else{
        block[LEN_IDX] = 0;
    }

    if(params->EDC == SC_EDC_LRC){
        block[len] = EDC_LRC(block, len);
        len++;
    }else{
        CRC = EDC_CRC(block, len);
        block[len++] = CRC >> 8;
        block[len++] = CRC & 0xFF;
    }

    *block_len = len;

    return sc_Status_Success;
}

static sc_Status    build_R_block                       (   iso_params_t*   params,
                                                            uint8_t         PCB_options,
                                                            uint8_t*        block,
                                                            uint32_t*       block_len
                                                        )
{
    uint8_t     len;
    uint16_t    CRC;

    len = 3;

    block[NAD_IDX] = (params->DAD << 4) | params->SAD;
    block[PCB_IDX] = PCB_R_BLOCK | (params->Nc << 4) | PCB_options;
    block[LEN_IDX] = 0;

    if(params->EDC == SC_EDC_LRC){
        block[len] = EDC_LRC(block, len);
        len++;
    }else{
        CRC = EDC_CRC(block, len);
        block[len++] = CRC >> 8;
        block[len++] = CRC & 0xFF;
    }

    *block_len = len;

    return sc_Status_Success;
}

static uint8_t      get_PCB_N                           (   uint8_t PCB
                                                        )
{
    switch(PCB & 0xC0){
    case PCB_S_BLOCK :
        return 0;
    case PCB_R_BLOCK :
        return (PCB >> 4) & 0x01;
    default :
        return (PCB >> 6) & 0x01;
    }
}

static uint8_t      get_PCB_type                        (   uint8_t PCB
                                                        )
{
    if ((PCB & 0x80) == PCB_I_BLOCK){
        return PCB_I_BLOCK;
    }
    return PCB & 0xC0;
}

static bool         check_resync_pcb                    (   uint8_t PCB
                                                        )
{
    if (get_PCB_type(PCB) != PCB_S_BLOCK)
        return false;
    if (ISSREQ(PCB))
        return false;
    if (STYPE(PCB) != PCB_S_RESYNC)
        return false;
    return true;
}

static sc_Status    protocol_APDU_T1_transact           (   sc_context_t*    context,
                                                            uint8_t*    send_buffer,
                                                            uint32_t    send_length,
                                                            uint8_t*    receive_buffer,
                                                            uint32_t*    receive_length
                                                        )
{
    sc_Status       ret;                            /* Return value */
    slot_itf_t*     slot;                           /* Reference on the slot interface */
    APDU_T1_state   state;                          /* State of the T1 APDU transaction */

    uint8_t         retries = 0, resyncs = 0;       /* Number of retries and resyncs */
    uint8_t         requested_ifsd;                 /* ifsd requested by upper layers */
    uint8_t         snd_block[T1_MAX_BLOCK_SIZE];   /* block buffer to send TPDU */
    uint8_t         rcv_block[T1_MAX_BLOCK_SIZE];   /* block buffer to receive TPDU */
    uint32_t        snd_len = 0, rcv_len;           /* TPDU length to send and receive */
    uint8_t         PCB, next_PCB = 0;              /* PCB of the received block and PCB of the next block to send */
    uint8_t*        S_data = 0;                     /* Pointer to data for S block, can be NULL*/
    uint32_t        len_to_send;                    /* Size of APDU to be sent */
    uint32_t        buffer_size;                    /* Size of the input receive buffer */

    /* Last Iblock correctly send/received */
    struct {
        uint8_t sender;
        uint8_t PCB;
        uint8_t LEN;
    } Last_I = {0,0,0};

    /* Initialize and verify parameters */
    slot = context->slot;
    buffer_size = *receive_length;
    len_to_send = send_length;
    PCB = 0;
    common_memset(snd_block, 0, T1_MAX_BLOCK_SIZE);
    common_memset(rcv_block, 0, T1_MAX_BLOCK_SIZE);

    if (    context == NULL ||
            send_buffer == NULL ||
            receive_buffer == NULL ||
            buffer_size < 4
        )
    {
        return sc_Status_Invalid_Parameter;
    }

    if ( context->params.state != sc_state_active_on_t1 ){
        return sc_Status_Bad_State;
    }

    /* Initialize transaction */
    ret        = sc_Status_Success;
    state    = APDU_T1_start_of_transaction;

    dbg_buff_comm("T1 APDU >> ", (char*)send_buffer, len_to_send);

    while (state != APDU_T1_exit){

        switch(state) {

        /**
         *  Start of transaction, checking that ifsd have to be negociated and send first Iblock
         */
        case APDU_T1_start_of_transaction   :

            retries    = 0;
            resyncs    = 0;
            send_length = 0;
            *receive_length = 0;
            S_data = NULL;

            /*check if ifsd of the slot is different from the transaction ifsd*/
            ret = slot->get_IFSD(&requested_ifsd);
            if (ret != sc_Status_Success){
                END_TRANSACTION(ret);
            }

            S_data = &requested_ifsd;
            next_PCB = PCB_S_BLOCK | PCB_S_IFS;

            state = APDU_T1_prepare_next_block;

            break;


        /**
         * Call TPDU layer and switch process according to received block
         */
        case APDU_T1_transact:

            rcv_len = sizeof(rcv_block);

            ret = protocol_TPDU_T1.Transact(context, snd_block, snd_len, rcv_block, &rcv_len);
            context->params.WTX = 0;

            /* Parity error */
            if ( (ret == sc_Status_Slot_Parity_Error) ||
                 (ret == sc_Status_TPDU_T1_Bad_EDC)){
                next_PCB = PCB_R_BLOCK | PCB_R_EDC_ERROR;
                state = APDU_T1_prepare_next_block;
                retries++;
                break;
            }

            /* Block error */
            if ( (ret == sc_Status_Slot_Reception_Timeout) ||
                 (ret == sc_Status_TPDU_T1_Bad_NAD) ||
                 (ret == sc_Status_TPDU_T1_Bad_PCB) ||
                 (ret == sc_Status_TPDU_T1_Bad_LEN))
            {
                next_PCB = PCB_R_BLOCK | PCB_R_OTHER_ERROR;
                state = APDU_T1_prepare_next_block;
                retries++;
                break;
            }

            /* Other error */
            if(ret != sc_Status_Success){
                END_TRANSACTION(ret);
            }

            PCB = rcv_block[PCB_IDX];

            /* resync or S requests must be aswered */
            if ((get_PCB_type(snd_block[PCB_IDX]) == PCB_S_BLOCK) &&
                (ISSREQ(snd_block[PCB_IDX])) &&
                (get_PCB_type(PCB) != PCB_S_BLOCK))
            {
                if (resyncs == 0)
                {
                    state = APDU_T1_prepare_next_block;
                    retries++;
                }
                else
                {
                    state = APDU_T1_resynch_request;
                }
                break;
            }

            /* Check received block type */
            switch(get_PCB_type(PCB)) {
                case PCB_I_BLOCK:
                state = APDU_T1_process_I_block;
                break;

                case PCB_R_BLOCK:
                state = APDU_T1_process_R_block;
                break;

                case PCB_S_BLOCK:
                state = APDU_T1_process_S_block;
                break;

                default:
                state = APDU_T1_resynch_request;
                break;
            }

            break;


        /**
         * Analyze I block and decide response
         */
        case APDU_T1_process_I_block :
            next_PCB = 0;

            /* If last I block was transmitted by the reader */
            if (Last_I.sender == READER){

                /* If last iblock send by the reader specified more block */
                if (HASMORE(Last_I.PCB)){
                    /* Card is not supposed to send I-block */
                    state = APDU_T1_resynch_request;
                    break;
                }

                /* Last I block don't specified more block */
                else{

                    /* If Ns != Nc , the send sequence number of the card*/
                    if (get_PCB_N(PCB) != context->params.Nc){
                        /* Wrong acknowledge from the card */
                        next_PCB = PCB_R_BLOCK | PCB_R_OTHER_ERROR;
                        state = APDU_T1_prepare_next_block;
                        retries++;
                        break;
                    }

                    /*if Ns == Nc */
                    else{
                        /* Ack from the card and data available */
                        /* Save last send length, update Nd*/
                        send_length += Last_I.LEN;
                        context->params.Nd ^= 0x01;

                        /* Update last i-block received */
                        Last_I.sender = CARD;
                        Last_I.PCB = rcv_block[PCB_IDX];
                        Last_I.LEN = rcv_block[LEN_IDX];

                        /* Update Nc */
                        context->params.Nc ^= 0x01;

                        /* Save data */
                        if(buffer_size < *receive_length + Last_I.LEN){
                            END_TRANSACTION(sc_Status_Buffer_To_Small);
                        }
                        common_memcpy(receive_buffer + *receive_length, rcv_block + T1_PROLOGUE_SIZE, Last_I.LEN);
                        *receive_length += Last_I.LEN;

                        /* If card has remaining data */
                        if(HASMORE(Last_I.PCB)){
                            next_PCB = PCB_R_BLOCK | PCB_R_ACK;
                            state = APDU_T1_prepare_next_block;
                            retries = 0;
                            break;
                        }

                        /* Transaction successfully ended */
                        else{
                            END_TRANSACTION(ret);
                        }
                    }
                }
            }

            /* If last block was transmitted by the card */
            if (Last_I.sender == CARD){

                /* Ns != Nc , the send sequence number of the card*/
                if (get_PCB_N(PCB) != context->params.Nc){
                    /* Error during transaction, request resync */
                    state = APDU_T1_resynch_request;
                    break;
                }

                /* Ns == Nc */
                else{
                    /* Update last i-block received */
                    Last_I.sender = CARD;
                    Last_I.PCB = rcv_block[PCB_IDX];
                    Last_I.LEN = rcv_block[LEN_IDX];

                    /* Update Nc */
                    context->params.Nc ^= 0x01;

                    /* Save data */
                    if(buffer_size < *receive_length + Last_I.LEN){
                        END_TRANSACTION(sc_Status_Buffer_To_Small);
                    }
                    common_memcpy(receive_buffer + *receive_length, rcv_block + T1_PROLOGUE_SIZE, Last_I.LEN);
                    *receive_length += Last_I.LEN;

                    /* If card has remaining data */
                    if(HASMORE(Last_I.PCB)){
                        next_PCB = PCB_R_BLOCK | PCB_R_ACK;
                        state = APDU_T1_prepare_next_block;
                        retries = 0;
                        break;
                    }

                    /* Transaction successfully ended */
                    else{
                        END_TRANSACTION(ret);
                    }
                }
            }

            /* Bad state */
            state = APDU_T1_resynch_request;
            break;


        /**
         * Analyze R block and decide response
         */
        case APDU_T1_process_R_block:
            next_PCB = 0;

            /* length is not 0 or bits 6,4 & 3 are not null*/
            if (( rcv_block[LEN_IDX] != 0x00 )    ||
                ( (PCB & 0x2C) != 0x00 ))
            {
                next_PCB = PCB_R_BLOCK | PCB_R_OTHER_ERROR;
                state = APDU_T1_prepare_next_block;
                retries++;
                break;
            }

            /* If last I block was transmitted by the reader */
            if (Last_I.sender == READER){

                /* If Nr != Ns of the last i-block*/
                if (get_PCB_N(PCB) != get_PCB_N(Last_I.PCB)){

                    /* If last iblock specified more block */
                    if (HASMORE(PCB)){
                        /* Chain data */
                        send_length += Last_I.LEN;
                        next_PCB = PCB_I_BLOCK;
                        context->params.Nd ^= 0x01;
                        state = APDU_T1_prepare_next_block;
                        retries = 0;
                        break;
                    }

                    /* If last I block don't specified more block */
                    else{
                        /* card had to ack, send R block */
                        next_PCB = PCB_R_BLOCK | PCB_R_OTHER_ERROR;
                        state = APDU_T1_prepare_next_block;
                        retries++;
                        break;
                    }
                }

                /*if Nr == Ns*/
                else{
                    /* Card failed to receive last I-block, retransmit it */
                    next_PCB = PCB_I_BLOCK;
                    state = APDU_T1_prepare_next_block;
                    retries++;
                    break;
                }
            }

            /* If last block was transmitted by the card */
            if (Last_I.sender == CARD){

                /* If Nr != Ns of the last I-block*/
                if (get_PCB_N(PCB) != get_PCB_N(Last_I.PCB)){

                    /* If last iblock specified more block */
                    if (HASMORE(PCB)){
                        /* Ack data */
                        next_PCB = PCB_R_BLOCK | PCB_R_ACK;
                        state = APDU_T1_prepare_next_block;
                        retries = 0;
                        break;
                    }

                    /* If last I block don't specified more block */
                    else{
                        /* card had to ack with I-block, send R block */
                        next_PCB = PCB_R_BLOCK | PCB_R_OTHER_ERROR;
                        state = APDU_T1_prepare_next_block;
                        retries++;
                        break;
                    }
                }

                /*if Nr == Ns*/
                else{
                    /* Invalid, need resync */
                    state = APDU_T1_resynch_request;
                    break;
                }
            }

            /* Bad state */
            state = APDU_T1_resynch_request;
            break;


        /**
         * Analyze S block and decide response
         */
        case APDU_T1_process_S_block:
            next_PCB = 0;

            if ((resyncs != 0) &&
                !check_resync_pcb(PCB))
            {
                state = APDU_T1_resynch_request;
                break;
            }

            /* The S block is a response */
            if (ISSRESP(rcv_block[PCB_IDX])){

                /* resync response */
                if (STYPE(rcv_block[PCB_IDX]) == PCB_S_RESYNC){

                    /* Response to our resync request */
                    if ( (rcv_block[LEN_IDX] == 0) &&
                         (snd_block[PCB_IDX] == (PCB_S_BLOCK | PCB_S_RESYNC)))
                    {
                        retries = 0;
                        resyncs = 0;
                        context->params.Nd = 0;
                        context->params.Nc = 0;
                        send_length = 0;
                        *receive_length = 0;
                        state = APDU_T1_start_of_transaction;
                        break;
                    }
                }

                /* ifs response */
                if (STYPE(rcv_block[PCB_IDX]) == PCB_S_IFS){

                    /* Response to our ifs request */
                    if ( (rcv_block[LEN_IDX] == 1)        &&
                         (rcv_block[3] == snd_block[3])
                         )
                    {
                        context->params.IFSD = snd_block[3];
                        next_PCB = PCB_I_BLOCK;
                        state = APDU_T1_prepare_next_block;
                        retries = 0;
                        break;
                    }
                }

                /* abort response */
                /* wtx response */
                /* Bad block */
            }

            /* The S block is a request */
            else{

                /* ifs request */
                if ( STYPE(rcv_block[PCB_IDX]) == PCB_S_IFS)
                {
                    if ((rcv_block[3] == 0) || (rcv_block[LEN_IDX] != 1)){
                        next_PCB = PCB_R_BLOCK | PCB_R_OTHER_ERROR;
                        state = APDU_T1_prepare_next_block;
                        retries++;
                        break;
                    }
                    context->params.IFSC = rcv_block[3];
                    next_PCB = rcv_block[PCB_IDX] | PCB_S_RESPONSE;
                    S_data = &rcv_block[3];
                    state = APDU_T1_prepare_next_block;
                    retries = 0;
                    break;
                }

                /* abort request */
                if ( STYPE(rcv_block[PCB_IDX]) == PCB_S_ABORT)
                {
                    if (rcv_block[LEN_IDX] != 0){
                        next_PCB = PCB_R_BLOCK | PCB_R_OTHER_ERROR;
                        state = APDU_T1_prepare_next_block;
                        retries++;
                        break;
                    }
                    next_PCB = rcv_block[PCB_IDX] | PCB_S_RESPONSE;
                    S_data = NULL;
                    state = APDU_T1_prepare_next_block;
                    retries = 0;
                    break;
                }

                /* wtx request */
                if (STYPE(rcv_block[PCB_IDX]) == PCB_S_WTX)
                {
                    if (rcv_block[LEN_IDX] != 1){
                        next_PCB = PCB_R_BLOCK | PCB_R_OTHER_ERROR;
                        state = APDU_T1_prepare_next_block;
                        retries++;
                        break;
                    }
                    context->params.WTX = rcv_block[3];
                    next_PCB = rcv_block[PCB_IDX] | PCB_S_RESPONSE;
                    S_data = &rcv_block[3];
                    state = APDU_T1_prepare_next_block;
                    retries = 0;
                    break;
                }

                /* resync request */
                /* Bad block */
            }

            /* Bad state */
            state = APDU_T1_resynch_request;
            break;


        /**
         * Prepare next block to send or go to resync
         */
        case APDU_T1_prepare_next_block :

            /* Too many retries regardless of resync mode — ISO 7816-3 §11.6.3 */
            if (retries > 2) {
                state = APDU_T1_resynch_request;
                break;
            }

            switch (get_PCB_type(next_PCB))
            {
                case PCB_I_BLOCK:
                {
                    ret = build_I_block(&(context->params), snd_block, &snd_len, send_buffer + send_length, len_to_send - send_length);

                    Last_I.sender = READER;
                    Last_I.PCB = snd_block[PCB_IDX];
                    Last_I.LEN = snd_block[LEN_IDX];
                    break;
                }

                case PCB_R_BLOCK:
                {
                    ret = build_R_block(&(context->params), next_PCB, snd_block, &snd_len);
                    break;
                }

                case PCB_S_BLOCK:
                {
                    ret = build_S_block(&(context->params), next_PCB, snd_block, &snd_len, S_data);
                    break;
                }
            }

            if (ret != sc_Status_Success){
                END_TRANSACTION(ret);
            }

            state = APDU_T1_transact;
            break;


        /*
         * Initiate a resync
         */
        case APDU_T1_resynch_request :
            if (resyncs > 2)
            {
                END_TRANSACTION(sc_Status_APDU_T1_Bad_Response);
            }

            next_PCB = PCB_S_BLOCK | PCB_S_RESYNC;
            S_data = NULL;
            state = APDU_T1_prepare_next_block;
            retries = 0;
            resyncs++;
            break;

        case APDU_T1_end_of_transaction :
            if (ret == sc_Status_Success){
                dbg_buff_comm("T1 APDU << ", (char*)receive_buffer, *receive_length);
            }

            state = APDU_T1_exit;
            break;

        case APDU_T1_exit    :
            /* Not supposed to append */
            return sc_Status_Bad_State;

        }

    }

    return ret;
}

/************************************************************************************
 * Public variables
 ************************************************************************************/

protocol_itf_t protocol_APDU_T1 = {
        protocol_APDU_T1_transact
};


/************************************************************************************
 * Public functions
 ************************************************************************************/
