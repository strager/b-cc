{-# LANGUAGE EmptyDataDecls #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module B.Foreign.QuestionVTableList where

import Foreign.C
import Foreign.Ptr

import B.Foreign.Question (QuestionVTable)
import B.Foreign.UUID (UUID)

data QuestionVTableList

foreign import ccall "b_question_vtable_list_allocate"
  allocate
    :: IO (Ptr QuestionVTableList)

foreign import ccall "b_question_vtable_list_deallocate"
  deallocate
    :: Ptr QuestionVTableList
    -> IO ()

foreign import ccall "b_question_vtable_list_add"
  add
    :: Ptr QuestionVTableList
    -> Ptr QuestionVTable
    -> IO ()

foreign import ccall "b_question_vtable_list_find_by_uuid"
  findByUUID
    :: Ptr QuestionVTableList
    -> UUID
    -> IO (Ptr QuestionVTableList)
