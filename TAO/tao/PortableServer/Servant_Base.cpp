// $Id$

#include "Servant_Base.h"
#include "POA.h"
#include "Operation_Table.h"
#include "Collocated_Object.h"

#include "tao/Timeprobe.h"
#include "tao/ORB_Core.h"
#include "tao/Stub.h"
#include "tao/Environment.h"
#include "tao/TAO_Server_Request.h"

ACE_RCSID(tao, Servant_Base, "$Id$")

#if !defined (__ACE_INLINE__)
# include "Servant_Base.i"
#endif /* ! __ACE_INLINE__ */

#if defined (ACE_ENABLE_TIMEPROBES)

static const char *TAO_Servant_Base_Timeprobe_Description[] =
{
  "Servant_Base::_find - start",
  "Servant_Base::_find - end"
};

enum
{
  TAO_SERVANT_BASE_FIND_START = 700,
  TAO_SERVANT_BASE_FIND_END
};

// Setup Timeprobes
ACE_TIMEPROBE_EVENT_DESCRIPTIONS (TAO_Servant_Base_Timeprobe_Description,
                                  TAO_SERVANT_BASE_FIND_START);

#endif /* ACE_ENABLE_TIMEPROBES */

TAO_ServantBase::TAO_ServantBase (void)
  : optable_ (0),
    single_threaded_poa_lock_ (0),
    single_threaded_poa_lock_count_ (0)
{
}

TAO_ServantBase::TAO_ServantBase (const TAO_ServantBase &rhs)
  : optable_ (rhs.optable_),
    single_threaded_poa_lock_ (0),
    single_threaded_poa_lock_count_ (0)
{
}

TAO_ServantBase &
TAO_ServantBase::operator= (const TAO_ServantBase &rhs)
{
  this->optable_ = rhs.optable_;
  return *this;
}

TAO_ServantBase::~TAO_ServantBase (void)
{
}

PortableServer::POA_ptr
TAO_ServantBase::_default_POA (CORBA::Environment &ACE_TRY_ENV)
{
  CORBA::Object_var object =
    TAO_ORB_Core_instance ()->root_poa (ACE_TRY_ENV);
  ACE_CHECK_RETURN (PortableServer::POA::_nil ());

  return PortableServer::POA::_narrow (object.in (),
                                       ACE_TRY_ENV);
}

CORBA::Boolean
TAO_ServantBase::_is_a (const char* logical_type_id,
                        CORBA::Environment &ACE_TRY_ENV)
{
  const char *id = CORBA::_tc_Object->id (ACE_TRY_ENV);
  ACE_CHECK_RETURN (0);

  if (ACE_OS::strcmp (logical_type_id, id) == 0)
    {
      return 1;
    }

  return 0;
}

CORBA::Boolean
TAO_ServantBase::_non_existent (CORBA::Environment &)
{
  return 0;
}

int
TAO_ServantBase::_find (const char *opname,
                        TAO_Skeleton& skelfunc,
                        const unsigned int length)
{
  ACE_FUNCTION_TIMEPROBE (TAO_SERVANT_BASE_FIND_START);
  return optable_->find (opname, skelfunc, length);
}

int
TAO_ServantBase::_bind (const char *opname,
                        const TAO_Skeleton skel_ptr)
{
  return optable_->bind (opname, skel_ptr);
}

TAO_Stub *
TAO_ServantBase::_create_stub (CORBA_Environment &ACE_TRY_ENV)
{
  TAO_Stub *stub = 0;

  TAO_POA_Current_Impl *poa_current_impl =
    ACE_static_cast(TAO_POA_Current_Impl *,
                    TAO_TSS_RESOURCES::instance ()->poa_current_impl_);

  CORBA::ORB_ptr servant_orb = 0;

  if (poa_current_impl != 0
      && this == poa_current_impl->servant ())
    {
      servant_orb = poa_current_impl->orb_core ().orb () ;


      stub = poa_current_impl->poa ()->key_to_stub (
          poa_current_impl->object_key (),
          this->_interface_repository_id (),
          poa_current_impl->priority (),
          ACE_TRY_ENV);
      ACE_CHECK_RETURN (0);
    }
  else
    {
      PortableServer::POA_var poa = this->_default_POA (ACE_TRY_ENV);
      ACE_CHECK_RETURN (0);

      CORBA::Object_var object = poa->servant_to_reference (this, ACE_TRY_ENV);
      ACE_CHECK_RETURN (0);

      // Get the stub object
      stub = object->_stubobj ();

      // Increment the reference count since <object> will zap its
      // stub object on deletion.
      stub->_incr_refcnt ();

      servant_orb = stub->orb_core ()->orb ();
    }

  stub->servant_orb (servant_orb);
  return stub;
}

ACE_SYNCH_MUTEX &
TAO_ServantBase::_single_threaded_poa_lock (void)
{
  return *this->single_threaded_poa_lock_;
}

void
TAO_ServantBase::_increment_single_threaded_poa_lock_count (void)
{
  // Only one thread at a time through this code (guarantee provided
  // by the POA).
  u_long current_count = this->single_threaded_poa_lock_count_++;

  if (current_count == 0)
    {
      ACE_NEW (this->single_threaded_poa_lock_,
               ACE_SYNCH_MUTEX);
    }
}

void
TAO_ServantBase::_decrement_single_threaded_poa_lock_count (void)
{
  // Only one thread at a time through this code (guarantee provided
  // by the POA).
  u_long current_count = --this->single_threaded_poa_lock_count_;

  if (current_count == 0)
    {
      delete this->single_threaded_poa_lock_;
      this->single_threaded_poa_lock_ = 0;
    }
}

void TAO_ServantBase::synchronous_upcall_dispatch (
    TAO_ServerRequest &req,
    void *context,
    void *derived_this,
    CORBA::Environment &ACE_TRY_ENV
  )
{
  TAO_Skeleton skel;
  const char *opname = req.operation ();

  // It seems that I might have missed s/g here.  What if
  // it is a one way that is SYNC_WITH_SERVER.
  // Add the following line to handle this reply send as well.

  // Handle the one ways that are SYNC_WITH_SERVER
  if (req.sync_with_server ())
    {
       req.send_no_exception_reply ();
    }

  // Fetch the skeleton for this operation
  if (this->_find(opname,skel,req.operation_length()) == -1)
    {
      ACE_THROW (CORBA_BAD_OPERATION());
    }

  ACE_TRY
  {
    // Invoke the skeleton, it will demarshal the arguments,
    // invoke the right operation on the skeleton class (<derived_this>).
    // and marshal any results
    skel (req, derived_this, context, ACE_TRY_ENV);
    ACE_TRY_CHECK;

    // It is our job to send the already marshaled reply, but only
    // send if it is expected and it has not already been sent

    // We send the reply only if it is NOT a SYNC_WITH_SERVER, a
    // response is expected and if the reply is not deferred.
    if ((!req.sync_with_server () &&
         req.response_expected () &&
         !req.deferred_reply ()))
      {
        req.tao_send_reply ();
      }

  }
  ACE_CATCH(CORBA::Exception,ex)
  {
    // If an exception was raised we should marshal it and send
    // the appropriate reply to the client
    req.tao_send_reply_exception(ex);
  }
  ACE_ENDTRY;
  ACE_CHECK;

  return;
}


TAO_RefCountServantBase::~TAO_RefCountServantBase (void)
{
}

void
TAO_RefCountServantBase::_add_ref (CORBA::Environment &)
{
  ++this->ref_count_;
}

void
TAO_RefCountServantBase::_remove_ref (CORBA::Environment &)
{
  CORBA::ULong new_count = --this->ref_count_;

  if (new_count == 0)
    {
      delete this;
    }
}

TAO_RefCountServantBase::TAO_RefCountServantBase (void)
  : ref_count_ (1)
{
}

TAO_RefCountServantBase::TAO_RefCountServantBase (const TAO_RefCountServantBase &)
  : ref_count_ (1)
{
}

TAO_RefCountServantBase &
TAO_RefCountServantBase::operator= (const TAO_RefCountServantBase &)
{
  return *this;
}

TAO_ServantBase_var::TAO_ServantBase_var (void)
  : ptr_ (0)
{
}

TAO_ServantBase_var::TAO_ServantBase_var (TAO_ServantBase *p)
  : ptr_ (p)
{
}

TAO_ServantBase_var::TAO_ServantBase_var (const TAO_ServantBase_var &b)
  : ptr_ (b.ptr_)
{
  if (this->ptr_ != 0)
    {
      this->ptr_->_add_ref ();
    }
}

TAO_ServantBase_var::~TAO_ServantBase_var (void)
{
  if (this->ptr_ != 0)
    {
      this->ptr_->_remove_ref ();
    }
}

TAO_ServantBase_var &
TAO_ServantBase_var::operator= (TAO_ServantBase *p)
{
  if (this->ptr_ != 0)
    {
      this->ptr_->_remove_ref ();
    }

  this->ptr_ = p;

  return *this;
}

TAO_ServantBase_var &
TAO_ServantBase_var::operator= (const TAO_ServantBase_var &b)
{
  if (this->ptr_ != b.ptr_)
  {
    if (this->ptr_ != 0)
      {
        this->ptr_->_remove_ref ();
      }

    if ((this->ptr_ = b.ptr_) != 0)
      {
        this->ptr_->_add_ref ();
      }
  }

  return *this;
}

TAO_ServantBase *
TAO_ServantBase_var::operator->() const
{
  return this->ptr_;
}

TAO_ServantBase *
TAO_ServantBase_var::in (void) const
{
  return this->ptr_;
}

TAO_ServantBase *&
TAO_ServantBase_var::inout (void)
{
  return this->ptr_;
}

TAO_ServantBase *&
TAO_ServantBase_var::out (void)
{
  if (this->ptr_ != 0)
    {
      this->ptr_->_remove_ref();
    }

  this->ptr_ = 0;

  return this->ptr_;
}

TAO_ServantBase *
TAO_ServantBase_var::_retn (void)
{
  TAO_ServantBase *retval = this->ptr_;
  this->ptr_ = 0;
  return retval;
}

void
TAO_Local_ServantBase::_dispatch (TAO_ServerRequest &,
                                  void *,
                                  CORBA_Environment &ACE_TRY_ENV)
{
  ACE_THROW (CORBA::BAD_OPERATION ());
}

#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)

template class ACE_Atomic_Op<ACE_SYNCH_MUTEX, long>;

#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)

#pragma instantiate ACE_Atomic_Op<ACE_SYNCH_MUTEX, long>

#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */
