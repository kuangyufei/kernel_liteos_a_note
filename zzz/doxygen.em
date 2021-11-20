//*****************************************************************************
// file    : doxygen.em
// It's a doxygen file
// ref: https://gist.github.com/Akagi201/3958206
// Copyright (c) 2011-2020  co. Ltd. All rights reserved
// 
// Change Logs:
// Date               Author      Note    
// 2018/05/08         kuan      First draft version
// 
//*****************************************************************************
//*****************************************************************************
//
//! \addtogroup doxygen
//! @{
//
//*****************************************************************************
/** @brief    AddVarUp
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddVarUp()
{
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()
    if( hbuf == hNil )
    {
        return 1
    }
    ln = GetBufLnCur( hbuf )
    tmp = GetBufLine(hbuf,ln)
    DelBufLine (hbuf, ln)
    tmp = cat( tmp, "/**  */" )
    
    InsBufLine( hbuf, ln , tmp )
    
    // put the insertion point inside the header comment
    SetBufIns( hbuf, ln , strlen(tmp)-3 )  
}
/** @brief    AddVarRight
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddVarRight()
{
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()
    if( hbuf == hNil )
    {
        return 1
    }
    ln = GetBufLnCur( hbuf )
    tmp = GetBufLine(hbuf,ln)
    DelBufLine (hbuf, ln)
    tmp = cat( tmp, "/**<  */" )
    
    InsBufLine( hbuf, ln , tmp )
    
    // put the insertion point inside the header comment
    SetBufIns( hbuf, ln , strlen(tmp)-3 )  
}
/** @brief    AddFuncHeader
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddFuncHeader()
{
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()
    if( hbuf == hNil )
    {
        return 1
    }
    ln = GetBufLnCur( hbuf )
    InsBufLine( hbuf, ln + 0, "/**" )
    InsBufLine( hbuf, ln + 1, "  * \brief " )
    InsBufLine( hbuf, ln + 2, "  *" )
    InsBufLine( hbuf, ln + 3, "  * \param " )
    InsBufLine( hbuf, ln + 4, "  *" )
    InsBufLine( hbuf, ln + 5, "  * \return " )
    InsBufLine( hbuf, ln + 6, "  */" )
    
    // put the insertion point inside the header comment
    SetBufIns( hbuf, ln + 0, 50 )
    
}
/** @brief    AddFuncHeaderAutoGenerate
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddFuncHeaderAutoGenerate()
{
    func_str = ""
    func_ln = 0
    tmp = ""
    
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()
    if( hbuf == hNil )
    {
        return 1
    }
    
    ln = GetBufLnCur(hbuf)
    ln_max=GetBufLineCount(hbuf)
    while(1)
    {
        tmp = GetBufLine(hbuf,ln)
        if(0 == strlen(Trim(tmp)))
        {
            ln = ln +1  
        }  
        else
        {
            break
        }  
        
        if(ln>=ln_max) 
        {
            return -1
        }
    }
    func_ln = ln
    while(1)
    {
        tmp=Trim(GetBufLine(hbuf,ln))
                     
        tmp_len=strlen(tmp)
       
        if(tmp_len>=1)
        {
            last_char=strmid(tmp,tmp_len-1,tmp_len)
        }
        else
        {
            last_char=""          
        }
        func_str = cat(func_str,tmp)
        if(")" == last_char||"}" == last_char)
        {
            break        
        }
        else
        {
            ln = ln +1                    
        } 
    }
    func_str=Trim(func_str)
    
    //func_str=GetStrRev(func_str)
    
    
    //InsBufLine( hbuf, func_ln, "@func_str@" ) 
    
    para_num = 0
    para_num_max =0 
    func_name = ""
    return_name=""
    para=""
   
    //func_name = ToFuncValidName(Trim(GetStrSplit(func_str,"("," ",0)))
    InsBufLine( hbuf, func_ln + 0, "/**" )  
    InsBufLine( hbuf, func_ln + 1, "  * \\brief @func_name@" )
    
    
    InsBufLine( hbuf, func_ln + 2, "  *" )
    
    empty_str=strmid(func_str,0,0)
    
    para_num_max=GetStrChrNum(func_str,",")
  
    if(0 == GetStrChrNum(func_str,","))
    {
        func_name = Trim(GetStrSplit(func_str,")","(",0))
        
        if("void" == func_name || "" == func_name)
        {
            InsBufLine( hbuf, func_ln + 3, "  * \\param None" ) 
            InsBufLine( hbuf, func_ln + 4, "  *" ) 
        }  
        else
        {
            para=DelStrPointer(Trim(GetStrSplit(func_str,")"," ",0)))
            InsBufLine( hbuf, func_ln + 3, "  * \\param @para@" ) 
            InsBufLine( hbuf, func_ln + 4, "  *" )       
        }
        InsBufLine( hbuf, func_ln + 4+(para_num_max)*2+1, "  * \\return " )
        InsBufLine( hbuf, func_ln + 4+(para_num_max)*2+2, "  */" ) 
    }
    else
    {    
        func_ln=func_ln-2
        while(1)
        {
            para = DelStrPointer(Trim(GetStrSplit(func_str,","," ",0)))           
            InsBufLine( hbuf, func_ln + 4+(para_num)*1+1, "  * \\param @para@" )           
            
            para_num=para_num+1
            if(para_num == para_num_max)
            {
                para=DelStrPointer(Trim(GetStrSplit(func_str,")"," ",0)))
                InsBufLine( hbuf, func_ln + 4+(para_num)*1+1, "  * \\param @para@" ) 
                InsBufLine( hbuf, func_ln + 4+(para_num)*1+2, "  *" )
                
                func_ln=func_ln+2
                InsBufLine( hbuf, func_ln + 4+(para_num_max)*1+1, "  * \\return " )
                InsBufLine( hbuf, func_ln + 4+(para_num_max)*1+2, "  */" )                 
                break;
            }
            else
            { 
                tmp_index=GetStrChrIndex(func_str,",")    
                func_str=strmid(func_str,tmp_index+1,strlen(func_str))             
            }
        }
    }
    func_ln=func_ln+2
}
/** @brief    AddFileHeader
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddFileHeader()
{
    szMyName = Ask( "[Add File Header] Please enter your name:" )  
    szModule = Ask( "[Add File Header] Please enter your module:" )  
    SysTime = GetSysTime( 0 )
    year  = SysTime.Year
    month = SysTime.Month
    day   = SysTime.Day
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()
    if( hbuf == hNil )
    {
        return 1
    }
    sz = GetFileName(GetBufName(hbuf))
    ln=0 
    InsBufLine( hbuf, ln,"//*****************************************************************************" )
    ln=ln+1   
    InsBufLine( hbuf, ln, "// file    : @sz@" )
    ln=ln+1
    InsBufLine( hbuf, ln, "// It's a @sz@ file" )
    ln=ln+1
    InsBufLine( hbuf, ln, "// " )
    ln=ln+1
    InsBufLine( hbuf, ln, "// Copyright (c) 2011-2020  co. Ltd. All rights reserved" )
    ln=ln+1
    InsBufLine( hbuf, ln, "// ")
    ln=ln+1
    InsBufLine( hbuf, ln, "// Change Logs:")
    ln=ln+1
    InsBufLine( hbuf, ln, "// Date               Author              Note    ")
    
    spaces="                                                                                                                   "
    len =0
    if(strlen(szMyName)>=20) 
    {
        len =1
    }
    else
    {
        len=strlen(szMyName)   
    }
    struct_member_space=strmid(spaces,0,20-len)
    if(month<10 && day <10)
    {
        ln=ln+1
        InsBufLine( hbuf, ln, "// @year@/0@month@/0@day@          @szMyName@@struct_member_space@First draft version")
    }
    else if(month<10 && day >10)
    {
        ln=ln+1
        InsBufLine( hbuf, ln, "// @year@/0@month@/@day@          @szMyName@@struct_member_space@First draft version")
    }
    else
    {
        ln=ln+1
        InsBufLine( hbuf, ln, "// @year@/@month@/@day@          @szMyName@@struct_member_space@First draft version")
    }
    ln=ln+1
    InsBufLine( hbuf, ln, "// " )
    ln=ln+1
    InsBufLine( hbuf, ln, "//*****************************************************************************" )
    
    ln=ln+1
    InsBufLine( hbuf, ln, "" )
    
    if("h"==GetFileNameExt(GetBufName(hbuf))
    {
        macro_str=""
        macro_str=GetFileNameNoExt(GetBufName(hbuf))  
        macro_str=cat(cat("__",toupper(macro_str)),"_H__")
        ln=ln+1
        InsBufLine( hbuf, ln, "#ifndef @macro_str@" )
        ln=ln+1
        InsBufLine( hbuf, ln, "#define @macro_str@" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "" )        
    }
          
    ln=ln+1
    InsBufLine( hbuf, ln, "//*****************************************************************************" )
    ln=ln+1
    InsBufLine( hbuf, ln, "//" )
    ln=ln+1
    InsBufLine( hbuf, ln, "//! \\addtogroup @szModule@" )
    ln=ln+1
    InsBufLine( hbuf, ln, "//! \@{" )
    ln=ln+1
    InsBufLine( hbuf, ln, "//" )
    ln=ln+1
    InsBufLine( hbuf, ln, "//*****************************************************************************" )
  
    ln=ln+1
    InsBufLine( hbuf, ln, "" )   
    if("h"==GetFileNameExt(GetBufName(hbuf))
    {
        ln=ln+1
        InsBufLine( hbuf, ln, "//*****************************************************************************" )
        ln=ln+1
        InsBufLine( hbuf, ln, "//" )
        ln=ln+1
        InsBufLine( hbuf, ln, "// If building with a C++ compiler, make all of the definitions in this header" )
        ln=ln+1
        InsBufLine( hbuf, ln, "// have a C binding." )
        ln=ln+1
        InsBufLine( hbuf, ln, "//")
        ln=ln+1
        InsBufLine( hbuf, ln, "//*****************************************************************************")
        ln=ln+1
        InsBufLine( hbuf, ln, "#ifdef __cplusplus")
        ln=ln+1
        InsBufLine( hbuf, ln, "extern \"C\"")
        ln=ln+1
        InsBufLine( hbuf, ln, "{")
        ln=ln+1
        InsBufLine( hbuf, ln, "#endif")
        ln=ln+1
        InsBufLine( hbuf, ln, "" )  

        hbuf = GetCurrentBuf()
        ln=GetBufLineCount(hbuf)    
        ln =ln-2
        
        ln=ln+1
        InsBufLine( hbuf, ln, "//*****************************************************************************" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "//" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "// Prototypes for the APIs." )   
        ln=ln+1
        InsBufLine( hbuf, ln, "//" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "//*****************************************************************************" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "//*****************************************************************************" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "//" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "// Mark the end of the C bindings section for C++ compilers." )   
        ln=ln+1
        InsBufLine( hbuf, ln, "//" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "//*****************************************************************************" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "#ifdef __cplusplus" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "}" )   
        ln=ln+1
        InsBufLine( hbuf, ln, "#endif" )   
        // ln=ln+1
        // InsBufLine( hbuf, ln, "" )   
    }
    
    ln =ln+2
    
    InsBufLine( hbuf, ln, "//*****************************************************************************" )
    ln=ln+1
    InsBufLine( hbuf, ln, "//" )
    ln=ln+1
    InsBufLine( hbuf, ln, "// Close the Doxygen group." )
    ln=ln+1
    InsBufLine( hbuf, ln, "//! \@{" )
    ln=ln+1
    InsBufLine( hbuf, ln, "//" )
    ln=ln+1
    InsBufLine( hbuf, ln, "//*****************************************************************************" )

    if("h"==GetFileNameExt(GetBufName(hbuf))
    {    
        macro_str=""
        macro_str=GetFileNameNoExt(GetBufName(hbuf))  
        macro_str=cat(cat("__",toupper(macro_str)),"_H__")
        ln=ln+1
        InsBufLine( hbuf, ln, "#endif //  @macro_str@ " )   
    }
}

/** @brief    AddStruct
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddStruct()
{  
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()

    if( hbuf == hNil )
    {
        return 1
    }
    
    ln = GetBufLnCur( hbuf )
    struct_name = Ask( "[Add Struct] Please enter your struct name:" )  
    
    
    InsBufLine( hbuf, ln,"/** describe @struct_name@ information */" )
    
    ln=ln+1
    InsBufLine( hbuf, ln,"struct @struct_name@" )
    ln=ln+1
    InsBufLine( hbuf, ln,"{" )
    spaces="                                                                                                                   "
    while(1)
    {
        struct_member = Ask( "[Add Struct] Please enter your struct member,if you enter \"end\" it will be end:" )  
        if(struct_member == "end") break;
        ln=ln+1
        struct_member_space=strmid(spaces,0,20-strlen(struct_member))
        struct_member_description = Ask( "[Add Struct] Please enter your struct member[@struct_member@] description" )  
        InsBufLine( hbuf, ln,"    @struct_member@;@struct_member_space@/**< @struct_member_description@@struct_member_space@*/" )
    }
    ln=ln+1
    InsBufLine( hbuf, ln,"};" )
    ln=ln+1
    InsBufLine( hbuf, ln,"" )
    ln=ln+1
    InsBufLine( hbuf, ln,"/** describe @struct_name@_t */" )
    ln=ln+1
    InsBufLine( hbuf, ln,"typedef struct @struct_name@ @struct_name@_t;" )
}

/** @brief    AddStructDemo
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddStructDemo()
{
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()

    if( hbuf == hNil )
    {
        return 1
    }
    ln = GetBufLnCur( hbuf )
    InsBufLine( hbuf, ln+0,"/** describe student information */" )
    InsBufLine( hbuf, ln+1,"struct student" )
    InsBufLine( hbuf, ln+2,"{" )
    InsBufLine( hbuf, ln+3,"    char *name;     /**< student's name */" )
    InsBufLine( hbuf, ln+4,"    int age;        /**< student's age */" )
    InsBufLine( hbuf, ln+5,"};" )
    InsBufLine( hbuf, ln+6,"" )
    InsBufLine( hbuf, ln+7,"/** describe student_t */" )
    InsBufLine( hbuf, ln+8,"typedef struct student student_t;" )
}


/** @brief    AddEnum
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddEnum()
{
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()

    if( hbuf == hNil )
    {
        return 1
    }
    
    ln = GetBufLnCur( hbuf )

    enum_name = Ask( "[Add enum] Please enter your enum name:" )  
    
    InsBufLine( hbuf, ln,"/** describe @enum_name@ information */" )
    
    ln=ln+1
    InsBufLine( hbuf, ln,"enum @enum_name@" )
    ln=ln+1
    InsBufLine( hbuf, ln,"{" )
    spaces="                                                                                                                   "
    while(1)
    {
        enum_member = Ask( "[Add enum] Please enter your enum member,if you enter \"end\" it will be end:" )  
        if(enum_member == "end") break;
        ln=ln+1
        enum_member_space=strmid(spaces,0,20-strlen(enum_member))
        enum_member_description = Ask( "[Add enum] Please enter your enum member[@enum_member@] description" )  
        InsBufLine( hbuf, ln,"    @enum_member@,@enum_member_space@/**< @enum_member_description@@enum_member_space@*/" )
    }
    
    ln=ln+1
    InsBufLine( hbuf, ln,"};" )
    ln=ln+1
    InsBufLine( hbuf, ln,"" )
    ln=ln+1
    InsBufLine( hbuf, ln,"/** define @enum_name@_t */" )
    ln=ln+1
    InsBufLine( hbuf, ln,"typedef enum @enum_name@ @enum_name@_t;" )
}

/** @brief    AddEnumDemo
  * @param[in]  
  * @param[out]  
  * @return  
  */
macro AddEnumDemo()
{
    // Get a handle to the current file buffer and the name
    // and location of the current symbol where the cursor is.
    hbuf = GetCurrentBuf()

    if( hbuf == hNil )
    {
        return 1
    }
    ln = GetBufLnCur( hbuf )
    InsBufLine( hbuf, ln+0,"/** describe color information */" )
    InsBufLine( hbuf, ln+1,"enum color" )
    InsBufLine( hbuf, ln+2,"{" )
    InsBufLine( hbuf, ln+3,"    E_RED,     /**< red color */" )
    InsBufLine( hbuf, ln+4,"    E_BLUE,    /**< blue color */" )
    InsBufLine( hbuf, ln+5,"    E_Yellow,  /**< yellow color */" )
    InsBufLine( hbuf, ln+6,"};" )
    InsBufLine( hbuf, ln+7,"" )
    InsBufLine( hbuf, ln+8,"/** describe color_t */" )
    InsBufLine( hbuf, ln+9,"typedef enum color color_t;" )
}

macro GetFileName(sz)
{
    i = 1
    szName = sz
    iLen = strlen(sz)
    if(iLen == 0)
      return ""
    while( i <= iLen)
    {
      if(sz[iLen-i] == "\\")
      {
        szName = strmid(sz,iLen-i+1,iLen)
        break
      }
      i = i + 1
    }
    return szName
}

macro GetFileNameExt(sz)
{
    i = 1
    j = 0
    szName = sz
    iLen = strlen(sz)
    if(iLen == 0)
      return ""
    while( i <= iLen)
    {
      if(sz[iLen-i] == ".")
      {
         j = iLen-i 
         szExt = strmid(sz,j + 1,iLen)
         return szExt
      }
      i = i + 1
    }
    return ""
}

macro GetFileNameNoExt(sz)
{
    i = 1
    szName = sz
    iLen = strlen(sz)
    j = iLen 
    if(iLen == 0)
      return ""
    while( i <= iLen)
    {
      if(sz[iLen-i] == ".")
      {
         j = iLen-i 
      }
      if( sz[iLen-i] == "\\" )
      {
         szName = strmid(sz,iLen-i+1,j)
         return szName
      }
      i = i + 1
    }
    szName = strmid(sz,0,j)
    return szName
}

macro TrimLeft(sz)  
{  
    i=0
    j=0  
    return_str=sz
    
    sz_len=strlen(sz)
    while( sz[i] == " "||sz[i] == "\t")
    {
        i=i+1  
        if(i>=sz_len)  break  
    }

    while(1) 
    {
        if(i>=sz_len)  break   
        return_str[j] = sz[i] 
        j=j+1
        i=i+1
   
    }
    return_str = strmid(return_str,0,j)
    return return_str
}  

macro TrimRight(sz)  
{  
    i=0
    return_str=sz
    
    if("" == return_str)
        return return_str
    
    sz_len=strlen(sz)
    i= sz_len-1
    
    while( sz[i] == " "||sz[i] == "\t") 
    {
        i=i-1  
        if(i<0)  break  
    }

    return_str = strmid(sz,0,i+1)
    return return_str
}  

macro Trim(sz)
{
    sz = TrimLeft(sz)
    sz = TrimRight(sz)
    
    return sz
}  


macro GetStrSplit(sz,chr_end,chr_start,start_index)  
{  
    i=0
    return_str=sz
    
    start_index=0
    end_index=0
   
    sz_len=strlen(sz)

    i=start_index
    while(1) 
    {
        if(sz[i] == chr_end)
        {
            end_index = i  
            break
        }         
        i=i+1
        if(i>=sz_len)
        {    
            end_index = -1
            break
        }                            
    }
    
    i=i-1 
    while( sz[i] == " "||sz[i] == "\t")
    {
        i=i-1  
        if(i<0)  break  
    }    
        
    while(1) 
    {
        if(sz[i] == chr_start)
        {
            start_index = i  
            break
        }         
        i=i-1
        if(i<0)
        {    
            start_index = -1
            break
        }                            
    }

    if(end_index != -1 && end_index>=start_index)
    {  
        return_str=strmid(sz,start_index+1,end_index)      
        return return_str
    }
    else
    {        
        return ""
    }
}  

macro GetStrSplitHeadStr(sz,chr_end,chr_start)  
{  
    i=0
    return_str=sz
    
    start_index=0
    end_index=0
   
    sz_len=strlen(sz)

    while(1) 
    {
        if(sz[i] == chr_end)
        {
            end_index = i  
            break
        }         
        i=i+1
        if(i>=sz_len)
        {    
            end_index = -1
            break
        }                            
    }
    
    i=i-1 
    while( sz[i] == " "||sz[i] == "\t") 
    {
        i=i-1  
        if(i<0)  break  
    }    
        
    while(1) 
    {
        if(sz[i] == chr_start)
        {
            start_index = i  
            break
        }         
        i=i-1
        if(i<0)
        {    
            start_index = -1
            break
        }                            
    }

    if(end_index != -1 && end_index>=start_index)
    {      
        return_str=strmid(sz,0,start_index)      
        return return_str
    }
    else
    {        
        return -1
    }
}  

macro GetStrChr(sz,chr)  
{  
    i=0
    return_str=sz
    
    index =0
   
    sz_len=strlen(sz)

    while(1) 
    {
        if(sz[i] == chr)
        {
            index = i  
            break
        }         
        i=i+1
        if(i>=sz_len)
        {    
            index = -1
            break
        }                            
    }
    
    if(index != -1)
    {  
        return_str=strmid(sz,index+1,sz_len)      
        return return_str
    }
    else
    {        
        return ""
    }
}

macro GetStrChrIndex(sz,chr)  
{  
    i=0
    return_str=sz
    
    index =0
   
    sz_len=strlen(sz)

    while(1) 
    {
        if(sz[i] == chr)
        {
            index = i  
            break
        }         
        i=i+1
        if(i>=sz_len)
        {    
            index = -1
            break
        }                            
    }
    
    return index

}
macro GetStrChrNum(sz,chr)  
{  
    i =0
    while(1)
    {
        sz = GetStrChr(sz,chr)
        if(sz == "")
        {
           break         
        }
        else
        {
            i=i+1          
        }  
    }
    return i
}

macro GetStrRev(sz)  
{  
    i=0
    return_str=sz
    
    start_index=0
    end_index=0
   
    sz_len=strlen(sz)

    while(1) 
    {      
        return_str[i]=sz[sz_len-i-1]
        i=i+1
        if(i>=sz_len)
        {    
            end_index = -1
            break
        }                            
    }
    return return_str
}

macro DelStrPointer(sz)  
{  
    i=0
    return_str=sz
    
    if(sz[0] == "*")
    {
        sz_len=strlen(sz)
        return_str=strmid(sz,1,sz_len)  
        return return_str        
    }
    else
    {
        return return_str
    }   
}

macro ToFuncValidName(sz)  
{  
    i=0
    return_str=sz
  
   
    sz_len=strlen(sz)

    while(1) 
    {
 
        if(return_str[i] == "_")
        {
            return_str[i] = " "
        }          
        i=i+1
        if(i>=sz_len)
        {    
            break
        }                            
    }
    return return_str
}  

//*****************************************************************************
//
// Close the Doxygen group.
//! @{
//
//*****************************************************************************