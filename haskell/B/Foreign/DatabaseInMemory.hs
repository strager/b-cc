{-# LANGUAGE EmptyDataDecls #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module B.Foreign.DatabaseInMemory where

import Foreign.C
import Foreign.Ptr

import B.Foreign.Database (AnyDatabase, DatabaseVTable)
import B.Foreign.Exception (Exception)
import B.Foreign.QuestionVTableList (QuestionVTableList)
import B.Foreign.Serialize (Deserializer, Serializer)

foreign import ccall "b_database_in_memory_allocate"
  allocate
    :: IO (Ptr AnyDatabase)

foreign import ccall "b_database_in_memory_deallocate"
  deallocate
    :: Ptr AnyDatabase
    -> IO ()

foreign import ccall "b_database_in_memory_vtable"
  vtable
    :: IO (Ptr DatabaseVTable)

foreign import ccall "b_database_in_memory_serialize"
  serialize
    :: Ptr a
    -> FunPtr (Serializer c)
    -> Ptr c
    -> IO ()

foreign import ccall "b_database_in_memory_deserialize"
  deserialize
    :: Ptr QuestionVTableList
    -> FunPtr (Deserializer c)
    -> Ptr c
    -> IO (Ptr a)

foreign import ccall "b_database_in_memory_recheck_all"
  recheckAll
    :: Ptr AnyDatabase
    -> Ptr (Ptr Exception)
    -> IO ()
