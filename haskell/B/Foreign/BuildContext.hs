{-# LANGUAGE EmptyDataDecls #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module B.Foreign.BuildContext where

import Foreign.C
import Foreign.Ptr

import B.Foreign.Answer (AnyAnswer)
import B.Foreign.Database (AnyDatabase, DatabaseVTable)
import B.Foreign.Exception (Exception)
import B.Foreign.Question (AnyQuestion, QuestionVTable)
import B.Foreign.Rule (AnyRule, RuleVTable)

data BuildContext

foreign import ccall "b_build_context_allocate"
  allocate
    :: Ptr AnyDatabase
    -> Ptr DatabaseVTable
    -> Ptr AnyRule
    -> Ptr RuleVTable
    -> IO (Ptr BuildContext)

foreign import ccall "b_build_context_deallocate"
  deallocate
    :: Ptr BuildContext
    -> IO ()

foreign import ccall "b_build_context_need_answers"
  needAnswers
    :: Ptr BuildContext
    -> Ptr (Ptr AnyQuestion)
    -> Ptr (Ptr QuestionVTable)
    -> Ptr (Ptr AnyAnswer)
    -> CSize
    -> Ptr (Ptr Exception)
    -> IO ()

foreign import ccall "b_build_context_need"
  need
    :: Ptr BuildContext
    -> Ptr (Ptr AnyQuestion)
    -> Ptr (Ptr QuestionVTable)
    -> CSize
    -> Ptr (Ptr Exception)
    -> IO ()

foreign import ccall "b_build_context_need_answer_one"
  needAnswerOne
    :: Ptr BuildContext
    -> Ptr AnyQuestion
    -> Ptr QuestionVTable
    -> Ptr (Ptr Exception)
    -> IO (Ptr AnyAnswer)

foreign import ccall "b_build_context_need_one"
  needOne
    :: Ptr BuildContext
    -> Ptr AnyQuestion
    -> Ptr QuestionVTable
    -> Ptr (Ptr Exception)
    -> IO ()
