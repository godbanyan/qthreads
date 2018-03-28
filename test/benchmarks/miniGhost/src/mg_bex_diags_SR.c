// ************************************************************************
//
//          miniGhost: stencil computations with boundary exchange.
//                 Copyright (2013) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Richard F. Barrett (rfbarre@sandia.gov) or
//                    Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************

// MPI internode communication strategies implemented in mg_bex_diags_<...>.
// 1) mg_bex_<diags>_SR:   MPI_Send / MPI_Recv
// 2) mg_bex_<diags>_ISR:  MPI_Isend/ MPI_Recv
// 3) mg_bex_<diags>_SIR:  MPI_Send / MPI_Irecv
// 4) mg_bex_<diags>_ISIR: MPI_Isend/ MPI_Irecv

#include "mg_tp.h"

int MG_Boundary_exchange_diags_SR ( InputParams params, MG_REAL *grid_in, BlockInfo blk, int ivar )
{
   // This is a coordinated exchange so that elements from diagonal neighbors automatically
   // propagates to the gathered halo. This requires completion of NORTH/SOUTH exchange,
   // then EAST/WEST, then FRONT/BACK.
 
   // ------------------
   // Local Declarations
   // ------------------

   int
      ierr = 0,                       // Return status
      count[3],
      bfd_xstart, bfd_xend,           // Offset to capture diagonal elements.
      bfd_ystart, bfd_yend,           // Offset to capture diagonal elements.
      ewd_start, ewd_end,             // Offset to capture diagonal elements.
      i, j, k, m,                     // Counters
      len,                            // Length required for msg buffers.
      msgtag[MAX_NUM_NEIGHBORS],
      num_recvs,                      // Counter
      offset,                         // Offset for packing and unpacking msg buffers.
      thread_id,
      which_neighbor;                 // Identifies incoming message.

   double
      time_start;

   MG_REAL
      *recvbuffer, 
      *sendbuffer;

   MPI_Status
      recv_status;

   // ---------------------
   // Executable Statements
   // ---------------------

   thread_id = mg_get_os_thread_num();

   int gd = params.ghostdepth;

   ierr = MG_Set_diag_comm ( params, blk, count, 
                             &bfd_xstart, &bfd_xend, &bfd_ystart, &bfd_yend, &ewd_start, &ewd_end );
   MG_Assert ( !ierr, "MG_Boundary_exchange_diags_SR:MG_Set_diag_comm" );

   ierr = MG_Get_tags ( params, blk, ivar, msgtag );
   MG_Assert ( !ierr, "MG_Boundary_exchange_SR::MG_Get_tags" );

   thread_id = mg_get_os_thread_num();

   // Assuming that these buffers cannot be permanently restricted to a particular task.

   len = count[NS];
   if ( count[EW] > len ) {
      len = count[EW];
   }
   if ( count[FB] > len ) {
      len = count[FB];
   }

   //MG_Barrier ( );
   //printf ( "[pe %d] len = %d \n", mgpp.mype, len );
   //MG_Barrier ( );
    
   recvbuffer = (MG_REAL*)MG_CALLOC ( len, sizeof(MG_REAL) );
   MG_Assert ( recvbuffer != NULL, "MG_Boundary_exchange_diags : MG_CALLOC ( recvbuffer )" );

   sendbuffer = (MG_REAL*)MG_CALLOC ( len, sizeof(MG_REAL) );
   MG_Assert ( sendbuffer != NULL, "MG_Boundary_exchange_diags : MG_CALLOC ( sendbuffer )" );

   //MG_Barrier ( );
   //printf ( "[pe %d] MG_BEX_DIAGS: blk.neighbors=(%d,%d) (%d,%d) (%d,%d) \n\n", mgpp.mype, 
            //blk.neighbors[NORTH], blk.neighbors[SOUTH], blk.neighbors[EAST], 
            //blk.neighbors[WEST], blk.neighbors[BACK], blk.neighbors[FRONT] );
   //MG_Barrier ( );
   
          //printf ( "[pe %d] HERE 1 (%d=%d,%d=%d) \n", mgpp.mype, blk.xstart-bfd_xstart,blk.yend+bfd_yend,blk.ystart-bfd_ystart,blk.yend+bfd_yend );

   // --------------------
   // North-south exchange
   // --------------------

   //int mm;
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {

   if ( blk.neighbors[NORTH] != -1 ) { 

      //printf ( "[pe %d] Have NORTH neighbor %d \n", mgpp.mype, blk.neighbors[NORTH] );
      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            sendbuffer[offset++] = grid_in(i, blk.yend, k);
             //printf ( "[pe %d] pack %4.2e for NORTH \n", mgpp.mype, grid_in(i, blk.yend, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_north[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Send ( sendbuffer, count[NS], MG_COMM_REAL, blk.neighbors[NORTH], 
                            msgtag[sNrS], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Send(NORTH)" );

      MG_Time_accum_l1(time_start,timings.send_north[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count[NS], MG_COMM_REAL, blk.neighbors[NORTH], 
                             msgtag[sSrN], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Recv(NORTH)" );

      MG_Time_accum_l1(time_start,timings.recv_north[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            grid_in(i, 0, k) = recvbuffer[offset++];
             //printf ( "[pe %d] UNPACK %4.2e from SOUTH \n", mgpp.mype, grid_in(i, 0, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.unpack_north[thread_id]);
   } 
   if ( blk.neighbors[SOUTH] != -1 ) {

      //printf ( "[pe %d] Have SOUTH neighbor %d \n", mgpp.mype, blk.neighbors[SOUTH] );
      MG_Time_start_l1(time_start);

      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count[NS], MG_COMM_REAL, blk.neighbors[SOUTH], 
                             msgtag[sNrS], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Recv(SOUTH)" );

      MG_Time_accum_l1(time_start,timings.recv_south[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            grid_in(i, params.ny + gd, k) = recvbuffer[offset++];
             //printf ( "[pe %d] UNPACK %4.2e from NORTH \n", mgpp.mype, grid_in(i, params.ny + gd, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.unpack_south[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( i=blk.xstart; i<=blk.xend; i++ ) {
            sendbuffer[offset++] = grid_in(i, gd, k);
             //printf ( "[pe %d] pack %4.2e for SOUTH \n", mgpp.mype, grid_in(i, gd, k) );
         }
      }
      MG_Time_start_l1(time_start);
      MG_Time_accum_l1(time_start,timings.pack_south[thread_id]);

      ierr = CALL_MPI_Send ( sendbuffer, count[NS], MG_COMM_REAL, blk.neighbors[SOUTH],
                            msgtag[sSrN], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Send(SOUTH)" );

      MG_Time_accum_l1(time_start,timings.send_south[thread_id]);
   } 

   //MG_Barrier ( );
   //printf ( "[pe %d] ************** Passed NS exchange. *********** \n", mgpp.mype );
   //MG_Barrier ( );
   
   // ------------------
   // East-west exchange
   // ------------------
   
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {
   if ( blk.neighbors[EAST] != -1 ) { 

      //printf ( "[pe %d] Have EAST neighbor %d \n", mgpp.mype, blk.neighbors[EAST] );

      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=ewd_start; j<=ewd_end; j++ ) {
            sendbuffer[offset++] = grid_in(blk.xend, j, k);
             //printf ( "[pe %d] pack %4.2e for EAST \n", mgpp.mype, grid_in(blk.xend, j, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_east[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Send ( sendbuffer, count[EW], MG_COMM_REAL, blk.neighbors[EAST], 
                            msgtag[sErW], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Send(EAST)" );

      MG_Time_accum_l1(time_start,timings.send_east[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count[EW], MG_COMM_REAL, blk.neighbors[EAST], 
                             msgtag[sWrE], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Recv(EAST)" );

      MG_Time_accum_l1(time_start,timings.recv_east[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=ewd_start; j<=ewd_end; j++ ) {
            grid_in(params.nx + gd, j, k) = recvbuffer[offset++];
             //printf ( "[pe %d] UNPACK %4.2e from EAST \n", mgpp.mype, grid_in(params.nx + gd, j, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.unpack_east[thread_id]);
   } 
   
   if ( blk.neighbors[WEST] != -1 ) { 

      //printf ( "[pe %d] Have WEST neighbor %d \n", mgpp.mype, blk.neighbors[WEST] );

      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count[EW], MG_COMM_REAL, blk.neighbors[WEST], 
                             msgtag[sErW], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Recv(WEST)" );

      MG_Time_accum_l1(time_start,timings.recv_west[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0;
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=ewd_start; j<=ewd_end; j++ ) {
            grid_in(0, j, k) = recvbuffer[offset++];
             //printf ( "[pe %d] UNPACK %4.2e from WEST \n", mgpp.mype, grid_in(0, j, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.unpack_west[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( k=blk.zstart; k<=blk.zend; k++ ) {
         for ( j=ewd_start; j<=ewd_end; j++ ) {
            sendbuffer[offset++] = grid_in(gd, j, k);
             //printf ( "[pe %d] pack %4.2e for WEST \n", mgpp.mype, grid_in(gd, j, k) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_west[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Send ( sendbuffer, count[EW], MG_COMM_REAL, blk.neighbors[WEST],
                            msgtag[sWrE], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Send(WEST)" );

      MG_Time_accum_l1(time_start,timings.send_west[thread_id]);
   } 
   //MG_Barrier ( );
   //printf ( "[pe %d] ************** Passed EW exchange. *********** \n", mgpp.mype );
   //MG_Barrier ( );

   // -------------------
   // Back-front exchange
   // -------------------
   
    //printf ( "[pe %d] HERE 3 (%d=%d,%d=%d) \n", mgpp.mype, blk.xstart-bfd_xstart,blk.yend+bfd_yend,blk.ystart-bfd_ystart,blk.yend+bfd_yend );

   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {
   if ( blk.neighbors[BACK] != -1 ) { 

      //printf ( "[pe %d] Have BACK neighbor %d \n", mgpp.mype, blk.neighbors[BACK] );

      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( j=bfd_ystart; j<=bfd_yend; j++ ) {
         for ( i=bfd_xstart; i<=bfd_xend; i++ )  {
            sendbuffer[offset++] = grid_in(i, j, blk.zend);
            //printf ( "[pe %d] pack %4.2e for BACK \n", mgpp.mype, grid_in(i, j, blk.zend) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_back[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Send ( sendbuffer, count[FB], MG_COMM_REAL, blk.neighbors[BACK],
                            msgtag[sBrF], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Send(BACK)" );

      MG_Time_accum_l1(time_start,timings.send_back[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count[FB], MG_COMM_REAL, blk.neighbors[BACK], 
                             msgtag[sFrB], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Recv(BACK)" );

      MG_Time_accum_l1(time_start,timings.recv_back[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0;
      for ( j=bfd_ystart; j<=bfd_yend; j++ ) {
         for ( i=bfd_xstart; i<=bfd_xend; i++ ) {
            grid_in(i, j, 0) = recvbuffer[offset++];
            //printf ( "[pe %d] UNPACK %4.2e from BACK \n", mgpp.mype, grid_in(i, j, 0) );
         }
      }
      MG_Time_accum_l1(time_start,timings.unpack_back[thread_id]);
   } 

   if ( blk.neighbors[FRONT] != -1 ) { 

      //printf ( "[pe %d] Have FRONT neighbor %d \n", mgpp.mype, blk.neighbors[FRONT] );

      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Recv ( recvbuffer, count[FB], MG_COMM_REAL, blk.neighbors[FRONT], 
                             msgtag[sBrF], MPI_COMM_MG, &recv_status );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Recv(FRONT)" );

      MG_Time_accum_l1(time_start,timings.recv_front[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0;
       //printf ( "[pe %d] UNPACKING offset=%d, (%d=%d,%d=%d) \n", mgpp.mype, offset, blk.xstart-bfd_xstart,blk.yend+bfd_yend,blk.ystart-bfd_ystart,blk.yend+bfd_yend );
      for ( j=bfd_ystart; j<=bfd_yend; j++ ) {
         for ( i=bfd_xstart; i<=bfd_xend; i++ ) {
            grid_in(i, j, params.nz + gd) = recvbuffer[offset++];
            //printf ( "[pe %d] UNPACK %4.2e from FRONT; offset=%d \n", mgpp.mype, grid_in(i, j, params.nz + gd), offset );
         }
      }
      MG_Time_accum_l1(time_start,timings.unpack_front[thread_id]);
      MG_Time_start_l1(time_start);

      offset = 0; 
      for ( j=bfd_ystart; j<=bfd_yend; j++ ) {
         for ( i=bfd_xstart; i<=bfd_xend; i++ ) {
            sendbuffer[offset++] = grid_in(i, j, gd);
            //printf ( "[pe %d] pack %4.2e for FRONT \n", mgpp.mype, grid_in(i, j, gd) );
         }
      }
      MG_Time_accum_l1(time_start,timings.pack_front[thread_id]);
      MG_Time_start_l1(time_start);

      ierr = CALL_MPI_Send ( sendbuffer, count[FB], MG_COMM_REAL, blk.neighbors[FRONT], 
                            msgtag[sFrB], MPI_COMM_MG );
      MG_Assert ( MPI_SUCCESS == ierr, "MG_Boundary_exchange_diags:CALL_MPI_Send(FRONT)" );

      MG_Time_accum_l1(time_start,timings.send_front[thread_id]);
   } 
   //}}
   //for ( mm=0; mm<mgpp.numpes; mm++ ) {
   //MG_Barrier ( );
   //if ( mm==mgpp.mype ) {

   //MG_Barrier ( );
   //printf ( "[pe %d] ************** Passed FB exchange. *********** \n", mgpp.mype );
   //MG_Barrier ( );

   // Boundary exchange complete, release workspace.
      
   if ( recvbuffer )
      free ( recvbuffer );

   if ( sendbuffer )
      free ( sendbuffer );

   return ( ierr );

} // End MG_Boundary_exchange_diags.
