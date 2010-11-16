/** @file distributed_table.h
 *
 *  @author Dongryeol Lee (dongryel@cc.gatech.edu)
 */

#ifndef CORE_TABLE_DISTRIBUTED_TABLE_H
#define CORE_TABLE_DISTRIBUTED_TABLE_H

#include <armadillo>
#include <new>
#include "boost/mpi.hpp"
#include "boost/mpi/collectives.hpp"
#include "boost/thread.hpp"
#include "boost/serialization/string.hpp"
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/random/variate_generator.hpp>
#include "core/table/table.h"
#include "core/table/mailbox.h"
#include "core/table/distributed_table_message.h"
#include "core/table/point_request_message.h"
#include "core/table/memory_mapped_file.h"
#include "core/tree/gen_metric_tree.h"

namespace core {
namespace table {

extern MemoryMappedFile *global_m_file_;

class DistributedTable: public boost::noncopyable {

  public:
    typedef core::tree::GeneralBinarySpaceTree < core::tree::GenMetricTree <
    core::table::DensePoint > > TreeType;

    typedef core::table::Table<TreeType> TableType;

  private:

    boost::interprocess::offset_ptr<core::table::TableInbox> table_inbox_;

    boost::interprocess::offset_ptr <
    core::table::TableOutbox<TableType> > table_outbox_;

    boost::interprocess::offset_ptr<TableType> owned_table_;

    boost::interprocess::offset_ptr<int> local_n_entries_;

    boost::interprocess::offset_ptr<TreeType> global_tree_;

    std::vector< TreeType * > global_tree_leaf_nodes_;

    int table_outbox_group_comm_size_;

  public:

    void UnlockPointinTableInbox() {
      table_inbox_->UnlockPoint();
    }

    void RunInbox(
      boost::mpi::communicator &table_outbox_group_comm_in,
      boost::mpi::communicator &table_inbox_group_comm_in,
      boost::mpi::communicator &computation_group_comm_in) {
      table_inbox_->Run(
        table_outbox_group_comm_in, table_inbox_group_comm_in,
        computation_group_comm_in);
    }

    void RunOutbox(
      boost::mpi::communicator &table_outbox_group_comm_in,
      boost::mpi::communicator &table_inbox_group_comm_in,
      boost::mpi::communicator &computation_group_comm_in) {
      table_outbox_->Run(
        table_outbox_group_comm_in, table_inbox_group_comm_in,
        computation_group_comm_in);
    }

    bool IsIndexed() const {
      return global_tree_ != NULL;
    }

    DistributedTable() {
      table_inbox_ = NULL;
      table_outbox_ = NULL;
      owned_table_ = NULL;
      local_n_entries_ = NULL;
      global_tree_ = NULL;
      table_outbox_group_comm_size_ = -1;
    }

    ~DistributedTable() {

      // Delete the mailboxes.
      if(table_outbox_ != NULL) {
        core::table::global_m_file_->DestroyPtr(table_outbox_.get());
        table_outbox_ = NULL;
      }
      if(table_inbox_ != NULL) {
        core::table::global_m_file_->DestroyPtr(table_inbox_.get());
        table_inbox_ = NULL;
      }

      // Delete the list of number of entries for each table in the
      // distributed table.
      if(local_n_entries_ != NULL) {
        if(core::table::global_m_file_) {
          core::table::global_m_file_->Deallocate(local_n_entries_.get());
        }
        else {
          delete[] local_n_entries_.get();
        }
        local_n_entries_ = NULL;
      }

      // Delete the table.
      if(owned_table_ != NULL) {
        if(core::table::global_m_file_) {
          core::table::global_m_file_->DestroyPtr(owned_table_.get());
        }
        else {
          delete owned_table_.get();
        }
        owned_table_ = NULL;
      }

      // Delete the tree.
      if(global_tree_ != NULL) {
        delete global_tree_.get();
        global_tree_ = NULL;
      }
    }

    const TreeType::BoundType &get_node_bound(TreeType * node) const {
      return node->bound();
    }

    TreeType::BoundType &get_node_bound(TreeType * node) {
      return node->bound();
    }

    TreeType *get_node_left_child(TreeType * node) {
      return node->left();
    }

    TreeType *get_node_right_child(TreeType * node) {
      return node->right();
    }

    bool node_is_leaf(TreeType * node) const {
      return node->is_leaf();
    }

    core::tree::AbstractStatistic *&get_node_stat(TreeType * node) {
      return node->stat();
    }

    int get_node_count(TreeType * node) const {
      return node->count();
    }

    TreeType *get_tree() {
      return global_tree_.get();
    }

    int n_attributes() const {
      return owned_table_->n_attributes();
    }

    int local_n_entries(int rank_in) const {
      if(rank_in >= table_outbox_group_comm_size_) {
        printf(
          "Invalid rank specified: %d. %d is the limit.\n",
          rank_in, table_outbox_group_comm_size_);
        return -1;
      }
      return local_n_entries_[rank_in];
    }

    int local_n_entries() const {
      return owned_table_->n_entries();
    }

    void Init(
      const std::string & file_name,
      boost::mpi::communicator &table_outbox_group_communicator_in) {

      // Initialize the table owned by the distributed table.
      owned_table_ = (core::table::global_m_file_) ?
                     core::table::global_m_file_->UniqueConstruct<TableType>() :
                     new TableType();
      owned_table_->Init(file_name);

      // Initialize the mailboxes.
      table_outbox_ = core::table::global_m_file_->UniqueConstruct <
                      core::table::TableOutbox<TableType> > ();
      table_inbox_ = core::table::global_m_file_->UniqueConstruct <
                     core::table::TableInbox > ();
      table_inbox_->Init(owned_table_->n_attributes());

      // Allocate the vector for storing the number of entries for all
      // the tables in the world, and do an all-gather operation to
      // find out all the sizes.
      table_outbox_group_comm_size_ = table_outbox_group_communicator_in.size();
      local_n_entries_ = (core::table::global_m_file_) ?
                         (int *) global_m_file_->ConstructArray<int>(
                           table_outbox_group_communicator_in.size()) :
                         new int[ table_outbox_group_communicator_in.size()];
      boost::mpi::all_gather(
        table_outbox_group_communicator_in, owned_table_->n_entries(),
        local_n_entries_.get());
    }

    void Save(const std::string & file_name) const {

    }

    void IndexData(
      const core::metric_kernels::AbstractMetric & metric_in,
      double sample_probability_in) {


    }

    void get(
      boost::mpi::communicator &table_outbox_group_comm_in,
      boost::mpi::communicator &table_inbox_group_comm_in,
      int requested_rank, int point_id,
      core::table::DenseConstPoint * entry) {

      // If owned by the process, just return the point. Otherwise, we
      // need to send an MPI request to the process holding the
      // required resource.
      if(table_outbox_group_comm_in.rank() == requested_rank) {
        owned_table_->get(point_id, entry);
      }

      // If the inbox has already fetched the point (do a cache
      // lookup) here, then no MPI call is necessary.
      else if(false) {

      }

      else {

        // The point request message.
        core::table::PointRequestMessage point_request_message(
          table_outbox_group_comm_in.rank(), point_id);

        // Inform the source processor that this processor needs data!
        table_outbox_group_comm_in.send(
          requested_rank,
          core::table::DistributedTableMessage::REQUEST_POINT_FROM_TABLE_OUTBOX,
          point_request_message);

        // Wait until the point has arrived.
        int dummy;
        table_inbox_group_comm_in.recv(
          table_outbox_group_comm_in.rank(),
          core::table::DistributedTableMessage::RECEIVE_POINT_FROM_TABLE_INBOX,
          dummy);

        // If we are here, then the point is ready. Alias the point.
        entry->Alias(
          table_inbox_->get_point(requested_rank, point_id),
          owned_table_->n_attributes());
      }
    }

    void PrintTree() const {
      global_tree_->Print();
    }
};
};
};

#endif
