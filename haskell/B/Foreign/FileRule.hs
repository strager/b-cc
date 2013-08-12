{-# LANGUAGE ForeignFunctionInterface #-}

module B.Foreign.FileRule where

import Foreign.C
import Foreign.Ptr

import B.Foreign.BuildContext (BuildContext)
import B.Foreign.Exception (Exception)
import B.Foreign.Rule (AnyRule, RuleVTable)

type FileRuleCallback
  = Ptr BuildContext -> CString -> Ptr (Ptr Exception) -> IO ()

foreign import ccall "b_file_rule_allocate"
  allocate
    :: IO (Ptr AnyRule)

foreign import ccall "b_file_rule_deallocate"
  deallocate
    :: Ptr AnyRule
    -> IO ()

foreign import ccall "b_file_rule_add"
  add
    :: Ptr AnyRule
    -> CString
    -> FunPtr FileRuleCallback
    -> IO ()

foreign import ccall "b_file_rule_add_many"
  addMany
    :: Ptr AnyRule
    -> Ptr CString
    -> CSize
    -> FunPtr FileRuleCallback
    -> IO ()

foreign import ccall "b_file_rule_vtable"
  vtable
    :: IO (Ptr RuleVTable)

foreign import ccall "wrapper"
  mkFileRuleCallback
    :: FileRuleCallback
    -> IO (FunPtr FileRuleCallback)
