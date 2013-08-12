{-# LANGUAGE ForeignFunctionInterface #-}

module B.Foreign.FileQuestion where

import Foreign.C
import Foreign.Ptr

import B.Foreign.Question (AnyQuestion, QuestionVTable)

foreign import ccall "b_file_question_allocate"
  allocate
    :: CString
    -> IO (Ptr AnyQuestion)

foreign import ccall "b_file_question_deallocate"
  deallocate
    :: Ptr AnyQuestion
    -> IO ()

foreign import ccall "b_file_question_file_path"
  filePath
    :: Ptr AnyQuestion
    -> IO CString

foreign import ccall "b_file_question_vtable"
  vtable
    :: IO (Ptr QuestionVTable)
