/* Programa basado en wxWidgets con funciones
    dedicadas al filtrado y/o procesado de imagenes:

        -Las modificaciones de la imagen son aplicadas
        sobre areas delimitidas por el usuario.

        -Las modificaciones pueden ser descartadas o recuperadas.

        -La imagen final puede ser exportada o regresar
        a su estado original.

    Programación y Algoritmos 
    M. en C. de Computacion
    Edgar Iván Aguilera Hernández
    29 / 11 /2024
*/

#include "wx/wx.h"
#include <wx/filedlg.h>
#include "wx/sizer.h"
#include "wx/rawbmp.h"
#include "wx/splitter.h"
#include "wx/spinctrl.h"
#include <iostream>
#include <ctime>

#define STACKSIZE 10    //Size of undo-redo operation stack

/*operations applied to image patches*/
class ImageProcess{
    private:
        wxBitmap patch;     //filtered image area
        wxBitmap old_patch; //pre-filtered image area
        int op_ID;          //index of operation applied
        int x, y;           //patch upperleft corner coordinate
        int w, h;           //patch size
        bool empty;         //verify if patch is allocated

    public:
        //constructors
        ImageProcess(){
            empty = true;
        }

        ImageProcess(wxImage &image, int operation, int x_coord, int y_coord, int width,int height){
            patch = image.GetSubImage(wxRect(x_coord,y_coord,width,height));
            //copy bitmap
            old_patch = patch.GetSubBitmap(wxRect(0, 0, patch.GetWidth(), patch.GetHeight()));
            op_ID = operation; 
            w = width;
            h = height;
            x = x_coord;
            y = y_coord;
            empty = false;
        }

        //getters
        bool getPatchState(){
            return empty;
        }

        int getOpID(){
            return op_ID;
        }

        int getX(){
            return x;
        }

        int getY(){
            return y;
        }

        int getWidth(){
            return w;
        }

        int getHeight(){
            return h;
        }


        //image processing methods
        wxBitmap setPatchImage(wxImage &image, int patch_mode);
        wxBitmap gauss_filter(wxImage &image);
        wxBitmap sobel_filter(wxImage &image);
        wxBitmap constrast(wxImage &image,double alpha, int beta);
        wxBitmap negative(wxImage &image);
        wxBitmap setOldPatchImage(wxImage &image);
};

///////////////////////////////////////////////////////////////////////Filtering Methods

wxBitmap ImageProcess::setPatchImage(wxImage &image, int patch_mode){
    //turn image into bitmap to handle pixels
    wxBitmap m_bitmap = wxBitmap(image);
    wxNativePixelData imageData(m_bitmap);
    if ( !imageData ){
        wxLogError("Error al generar mapa de bits");
        return m_bitmap.ConvertToImage();
    }

    //turn patch into nativepixel object to handle pixels
    wxNativePixelData *patchData;
    if(patch_mode)
        patchData = new wxNativePixelData(patch);
    else
        patchData = new wxNativePixelData(old_patch);
    if ( !patchData ){
        wxLogError("Error al generar mapa de bits");
        return m_bitmap.ConvertToImage();
    }

    //iterate over every pixel in the patch
    wxNativePixelData::Iterator pImage(imageData);
    wxNativePixelData::Iterator pPatch(*patchData);

    pImage.MoveTo(imageData,x,y);
    for ( int y = 0; y < h; ++y ){
        wxNativePixelData::Iterator image_rowStart = pImage;
        wxNativePixelData::Iterator patch_rowStart = pPatch;

        for ( int x = 0; x < w; ++x ){
            //printf("[%d,%d] image: %d, patch: %d\n",y,x,pImage.Red(),pPatch.Red());
            //set value of the patch into whole image
            pImage.Red() = pImage.Green() = pImage.Blue() = pPatch.Red();

            ++pImage; // advance to next pixel to the Right
            ++pPatch; 
        }

        pImage = image_rowStart;
        pPatch = patch_rowStart;
        pImage.OffsetY(imageData, 1);
        pPatch.OffsetY(*patchData, 1);
    }

    delete patchData;
    return m_bitmap;
}


/*Apply gaussian filter for smoother image*/
wxBitmap ImageProcess::gauss_filter(wxImage &image){
    //Gauss Kernel 5x5 window
    int kernel[5][5] = {{1,4,7,4,1},
                        {4,16,26,16,4},
                        {7,26,41,26,7},
                        {4,16,26,16,4},
                        {1,4,7,4,1}};
    int f_rows = 5,f_cols = 5;
    //filter matrix center
    int center_i = f_rows/2;
    int center_j = f_cols/2;

    //window mutiply accumulator and division coefficient
    int aux;
    int div_c = 0;

    //calculate division coeficient
    for(int i= 0; i < f_rows*f_cols; i++){
        div_c += *(kernel[0] + i);
    }

    //turn patch image into bitmap to handle pixels
    wxNativePixelData data_filter(patch);
    wxNativePixelData data_original(old_patch);
    if ( !data_filter |  !data_original){
        wxLogError("Failed to gain raw access to bitmap data");
        return image;
    }


    //iterator over patches pixels
    wxNativePixelData::Iterator p_filter(data_filter);
    wxNativePixelData::Iterator p_original(data_original);
    for ( int y = 0; y < h; y++ ){
        wxNativePixelData::Iterator filter_rowStart = p_filter;
        for ( int x = 0; x < w; x++ ){
            //reset Channel values
            aux = 0;

            //filter window coordinates
            p_original.MoveTo(data_original,x-center_j,y-center_i);
            for(int i = y-center_i; i <= y+center_i; i++){
                wxNativePixelData::Iterator original_rowStart = p_original;
                for(int j = x-center_j; j <= x+center_j; j++){
                    
                    //exclude non fitting filter pixels
                    if((i<0) || (j<0) || (i>=h) || (j>=w)){
                        break;
                    }
                    
                    //take any of the color channel value
                    aux += p_original.Red() * kernel[center_i-(y-i)][center_j-(x-j)];

                    ++p_original; // advance to next pixel to the right
                }
                p_original = original_rowStart;
                p_original.OffsetY(data_original, 1);
            }

            p_filter.Red() = p_filter.Green() = p_filter.Blue() = aux / div_c;
            ++p_filter; // advance to next pixel to the right
        }
        p_filter = filter_rowStart;
        p_filter.OffsetY(data_filter, 1);
    }
    
    return  setPatchImage(image,1);
}

/*Apply Sobel filter for border detection */
wxBitmap ImageProcess::sobel_filter(wxImage &image){
    //Sobel kernel
    int sx[3][3] = { {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} };
    int sy[3][3] = { {1, 2, 1}, {0, 0, 0}, {-1, -2, -1} };
    //kernel center
    int center_i = 1;
    int center_j = 1;

    //window mutiply accumulator
    int aux_gx, aux_gy, res;

    //turn patch image into bitmap to handle pixels
    wxNativePixelData data_filter(patch);
    wxNativePixelData data_original(old_patch);
    if ( !data_filter |  !data_original){
        wxLogError("Error al generar mapa de bits");
        return image;
    }

    //iterator over patches pixels
    wxNativePixelData::Iterator p_filter(data_filter);
    wxNativePixelData::Iterator p_original(data_original);
    //center pixel of image
    for(int y = 0; y < h; y++){
        wxNativePixelData::Iterator filter_rowStart = p_filter;
        for(int x = 0; x < w; x++){
            aux_gx = 0;
            aux_gy = 0;

            //filter window coordinates
            p_original.MoveTo(data_original,x-center_j,y-center_i);
            for(int i= y-center_i; i <= y+center_i; i++){
                wxNativePixelData::Iterator original_rowStart = p_original;
                for(int j = x-center_j; j <= x+center_j; j++){
                    //exclude non fitting filter pixels
                    if((i<0) || (j<0) || (i>=h) || (j>=w)){
                        break;
                    }
                    
                    //take any of the color channel value
                    aux_gx += p_original.Red() * sx[center_i-(y-i)][center_j-(x-j)];
                    aux_gy += p_original.Red() * sy[center_i-(y-i)][center_j-(x-j)];

                    ++p_original; // advance to next pixel to the right
                }
                p_original = original_rowStart;
                p_original.OffsetY(data_original, 1);
            }

            res = (unsigned char)(sqrt(aux_gx*aux_gx + aux_gy*aux_gy));
            p_filter.Red() = p_filter.Green() = p_filter.Blue() = res;

            ++p_filter; // advance to next pixel to the right
        }
        p_filter = filter_rowStart;
        p_filter.OffsetY(data_filter, 1);
    }

    return setPatchImage(image,1);
}

/*Change constrast factor (alpha) and ilumination (beta) */
wxBitmap ImageProcess::constrast(wxImage &image,double alpha, int beta){
    //turn patch image into bitmap to handle pixels
    wxNativePixelData data_filter(patch);

    if ( !data_filter ){
        wxLogError("Error al generar mapa de bits");
        return image;
    }

    //iterator over patches pixels
    wxNativePixelData::Iterator p_filter(data_filter);

    //center pixel of image
    for(int y = 0; y < h; y++){
        wxNativePixelData::Iterator filter_rowStart = p_filter;
        for(int x = 0; x < w; x++){
                    
            //Change all channels at once using one as reference
            p_filter.Red() = p_filter.Green() = p_filter.Blue() = (int)(p_filter.Red() * alpha) + beta;
            //8 bit limit (value clipping)
            if(p_filter.Red() > 255){
                p_filter.Red() = p_filter.Green() = p_filter.Blue() = 255;
            }
            ++p_filter; // advance to next pixel to the right
        }
        p_filter = filter_rowStart;
        p_filter.OffsetY(data_filter, 1);
    }

    return setPatchImage(image,1);
}

/*Change values to their difference from the max value (255) */
wxBitmap ImageProcess::negative(wxImage &image){
    //turn patch image into bitmap to handle pixels
    wxNativePixelData data_filter(patch);

    if ( !data_filter ){
        wxLogError("Error al generar mapa de bits");
        return image;
    }

    //iterator over patches pixels
    wxNativePixelData::Iterator p_filter(data_filter);

    //center pixel of image
    for(int y = 0; y < h; y++){
        wxNativePixelData::Iterator filter_rowStart = p_filter;
        for(int x = 0; x < w; x++){
                    
            //Change all channels at once using complement of any of them
            p_filter.Red() = p_filter.Green() = p_filter.Blue() = 255 ^ p_filter.Red();
            ++p_filter; // advance to next pixel to the right
        }
        p_filter = filter_rowStart;
        p_filter.OffsetY(data_filter, 1);
    }

    return setPatchImage(image,1);
}


/*stack of operations applied to the original image*/
class operationStack{
    private:
        ImageProcess stack[STACKSIZE];
        int top;

        /*eliminate first element from the stack to push another*/
        void freeStackOverflow(){
            ImageProcess aux;
            for(int i = 0; i < STACKSIZE-1; i++){
                stack[i] = stack[i+1];
            }
            top--;
        }

    public:
        operationStack(){
            top = -1;
            for(int i = 0; i < STACKSIZE; i++){
                stack[i] = ImageProcess();
            }
        }

        int getElements(){
            return top+1;
        }

        /*Insert image operation into the stack*/
        void push(ImageProcess data){
            //verify full stack
            if(top == STACKSIZE-1){
                printf("La pila se encuentra llena\n");
                freeStackOverflow();
            }

            //update top 
            top++;
            //insert data at the top
            stack[top] = data;
   
        }

        /*get image operation from the top*/
        ImageProcess pop(){
            //verify empty stack
            if(top == -1){
                return ImageProcess();
            }

            //get data from the stack
            ImageProcess topData = stack[top];
            //update top 
            top--;

            return topData;
        }
};

/*Image panel handler*/
class wxImagePanel : public wxPanel
{
    wxImage image;
    wxBitmap m_bitmap;
    wxBitmap resized;
    int w, h;
    
public:
    wxImagePanel(wxSplitterWindow *parent, wxString file, wxBitmapType format);
    wxImagePanel(wxSplitterWindow *parent);
    void setImage(wxString file, wxBitmapType format);
    void setImage(wxBitmap new_bitMap, wxBitmapType format);
    wxImage getImage();
    int getWidth();
    int getHeight();
    void paintEvent(wxPaintEvent & evt);
    void paintNow();
    void OnSize(wxSizeEvent& event);
    void render(wxDC& dc);
    //static event handling
    DECLARE_EVENT_TABLE();
};


BEGIN_EVENT_TABLE(wxImagePanel, wxPanel)
    EVT_PAINT(wxImagePanel::paintEvent)// catch paint events
    EVT_SIZE(wxImagePanel::OnSize)//Size event
END_EVENT_TABLE()

/*constructor with default local image (test purposes)*/
wxImagePanel::wxImagePanel(wxSplitterWindow *parent, wxString file, wxBitmapType format) :
wxPanel(parent){
    
    m_bitmap.LoadFile(file, format);
    image = wxImage(m_bitmap.ConvertToImage());
    w = image.GetWidth();
    h = image.GetHeight();
}

/*starting program constructor*/
wxImagePanel::wxImagePanel(wxSplitterWindow *parent):wxPanel(parent){
    //default black image of size 100 x 100
    image = wxImage(100,100,true);
    w = image.GetWidth();
    h = image.GetHeight();
}

/*set new image loaded from path*/
void wxImagePanel::setImage(wxString file, wxBitmapType format){
    
    m_bitmap.LoadFile(file, format);
    image = wxImage(m_bitmap.ConvertToImage());
    w = image.GetWidth();
    h = image.GetHeight();
}

/*set new bitmap as a result of any process*/
void wxImagePanel::setImage(wxBitmap new_bitMap, wxBitmapType format){

    m_bitmap = new_bitMap;
    image = wxImage(new_bitMap.ConvertToImage());
    w = image.GetWidth();
    h = image.GetHeight();
}

/*getters*/
wxImage wxImagePanel::getImage(){
    return image;
}

int wxImagePanel::getWidth(){
    return w;
}

int wxImagePanel::getHeight(){
    return h;
}

/*Refresh the image whith any change or event associated (triggered manually by calling Refresh()/Update())*/
void wxImagePanel::paintEvent(wxPaintEvent & evt){
    wxPaintDC dc(this);
    render(dc);
}


void wxImagePanel::paintNow(){
    wxClientDC dc(this);
    render(dc);
}


void wxImagePanel::render(wxDC&  dc){
    int neww, newh;
    dc.GetSize( &neww, &newh );
    
    if( neww != w || newh != h )
    {
        resized = wxBitmap( image.Scale( neww, newh /*, wxIMAGE_QUALITY_HIGH*/ ) );
        w = neww;
        h = newh;
        dc.DrawBitmap( resized, 0, 0, false );
    }else{
        dc.DrawBitmap( resized, 0, 0, false );
    }
}

/*tell the panel to draw itself again (when the user resizes the image panel)*/
void wxImagePanel::OnSize(wxSizeEvent& event){
    Refresh();
    //skip the event.
    event.Skip();
}

/*pointer to member functions using order of declaration in the listBox*/
typedef wxBitmap(ImageProcess::*Functionsptr)(wxImage &image);
 
class MyFrame : public wxFrame{
    public:
        MyFrame(wxBoxSizer *sizer);

    private:
        Functionsptr opArr[3] = {&ImageProcess::sobel_filter,   //array of pointers to functions
                                &ImageProcess::negative,
                                &ImageProcess::gauss_filter};
        operationStack undoStack;                               //stack instance for undo function
        operationStack redoStack;                               //stack instance for redo function
        int XYLimit[2] = {0,0};                                 //Array for max limits on spincontrols
        wxImagePanel *drawPanel;                                //instance of panel where image is displayed
        wxPanel *optionPanel;                                   //instance of panel where options are displayed
        wxPanel *logPanel;                                      //instance of panel where logbox is displayed
        wxButton *undoBtn;                                      //pointer to instance of "deshacer" button
        wxButton *redoBtn;                                      //pointer to instance of "rehacer" button
        wxListBox *filterList;                                  //pointer to instance of listbox 
        wxSpinCtrl *xUpperLeft;                                 //pointer to instance of "X" spin control
        wxSpinCtrl *yUpperLeft;                                 //pointer to instance of "Y" spin control
        wxSpinCtrl *width;                                      //pointer to instance of "W" spin control
        wxSpinCtrl *height;                                     //pointer to instance of "H" spin control
        wxSpinCtrlDouble *alpha;                                //pointer to instance of "alpha" double spin control
        wxSpinCtrl *beta;                                       //pointer to instance of "beta" spin control
        wxButton *apply;                                        //pointer to instance of "aplicar" button
        wxTextCtrl *textlog;                                    //pointer to instance of "Log" textbox


        void setTextInLog(wxString logMessage);
        void resetSpinCtrls();
        void updateUndoRedo();

        //static event handling
        void OnOpen(wxCommandEvent& event);
        void OnSave(wxCommandEvent& event);
        void OnExit(wxCommandEvent& event);
        void OnAbout(wxCommandEvent& event);
        void OnButtonUndoClick(wxCommandEvent& event);
        void OnButtonRedoClick(wxCommandEvent& event);
        void OnListBoxSelection(wxCommandEvent& event);
        void OnXULSpinChange(wxCommandEvent& event);
        void OnYULSpinChange(wxCommandEvent& event);
        void OnButtonApplyClick(wxCommandEvent& event);
        DECLARE_EVENT_TABLE();
        
};
 

/*Element's label assignation*/
enum{
    ID_Open = 1,
    ID_Save = 2,
    BUTTON1 = 3,
    BUTTON2 = 4,
    LISTBOX = 5,
    SPINCTRL1 = 6,
    SPINCTRL2 = 7,
    SPINCTRL3 = 8,
    SPINCTRL4 = 9,
    SPINCTRLD = 10,
    SPINCTRL5 = 11,
    BUTTON3 = 12,
    TEXTBOX = 13
};

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(ID_Open,MyFrame::OnOpen)
    EVT_MENU(ID_Save,MyFrame::OnSave)
    EVT_MENU(wxID_ABOUT,MyFrame::OnAbout)
    EVT_MENU(wxID_EXIT,MyFrame::OnExit)
    EVT_BUTTON(BUTTON1,MyFrame::OnButtonUndoClick)
    EVT_BUTTON(BUTTON2,MyFrame::OnButtonRedoClick)
    EVT_LISTBOX(LISTBOX,MyFrame::OnListBoxSelection)
    EVT_BUTTON(BUTTON3,MyFrame::OnButtonApplyClick)
END_EVENT_TABLE()

//////////////////////////////////////////////////////////////////window elements initialization


/*Elemnts initialization associated with the main window*/
MyFrame::MyFrame(wxBoxSizer *sizer)
    : wxFrame(nullptr, wxID_ANY, "Proyecto P y A I",wxPoint(1200,1200), wxSize(1200,700)){
    //initialize operations history stacks
    undoStack = operationStack();
    redoStack = operationStack();

    //initialize top menu
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(ID_Open, "&Abrir...\tCtrl-O","Abrir imagen");
    menuFile->AppendSeparator();
    menuFile->Append(ID_Save, "&Guardar...\tCtrl-S","Guardar como nueva imagen");
    menuFile->Append(wxID_EXIT,"Salir","Cerrar programa");
 
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
 
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar( menuBar );

    //add splitters for better UX
    wxSplitterWindow *splitter = new wxSplitterWindow(this,wxID_ANY, wxDefaultPosition,wxDefaultSize,
                                wxSP_BORDER | wxSP_LIVE_UPDATE);
    
    wxSplitterWindow *rightsplitter = new wxSplitterWindow(splitter,wxID_ANY, wxDefaultPosition,wxSize(650,200),
                                wxSP_BORDER | wxSP_LIVE_UPDATE);
    
    optionPanel = new wxPanel(splitter);
    /*drawPanel = new wxImagePanel(rightsplitter, 
                wxT("/home/edgarbanzo/Programs/PyA/Proyecto/resources/barbara_ascii.pgm"), wxBITMAP_TYPE_PNM);*/
    drawPanel = new wxImagePanel(rightsplitter);
    sizer = new wxBoxSizer(wxHORIZONTAL);
    rightsplitter->SetSizer(sizer);
    sizer->Add(drawPanel, 1, wxEXPAND);

    logPanel = new wxPanel(rightsplitter);

    splitter->SetMinimumPaneSize(200);
    splitter->SplitVertically(optionPanel,rightsplitter);
    rightsplitter->SetMinimumPaneSize(150);
    rightsplitter->SplitHorizontally(drawPanel,logPanel);
    rightsplitter->SetSashPosition(-50);
    rightsplitter->SetSashGravity(1);

    //get image size
    XYLimit[0] = drawPanel->getWidth();
    XYLimit[1] = drawPanel->getHeight();

    //buttons initialization
    undoBtn = new wxButton(optionPanel,BUTTON1,_T("Deshacer"),wxPoint(10,10));
    undoBtn->SetBackgroundColour(wxColour(117, 240, 230));
    undoBtn->Disable();
    redoBtn = new wxButton(optionPanel,BUTTON2,_T("Rehacer"),wxPoint(200,10));
    redoBtn->SetBackgroundColour(wxColour(117, 240, 230));
    redoBtn->Disable();
    apply = new wxButton(optionPanel,BUTTON3,_T("Aplicar"),wxPoint(350,300));
    apply->SetBackgroundColour(wxColour(117, 240, 230));
    apply->Disable();
    wxString choices[] = {_T("Bordes"),
                        _T("Invertir"),
                        _T("Suavizado"),
                        _T("Contraste")};
    filterList = new wxListBox(optionPanel,LISTBOX,wxPoint(10,100), wxSize(125,100),
                        4, choices, wxLB_SINGLE);
    
    xUpperLeft = new wxSpinCtrl(optionPanel,SPINCTRL1,"0",wxPoint(170,100),wxSize(125,34));
    xUpperLeft->SetRange(0,XYLimit[0]-1);
    yUpperLeft = new wxSpinCtrl(optionPanel,SPINCTRL2,"0",wxPoint(350,100),wxSize(125,34));
    yUpperLeft->SetRange(0,XYLimit[1]-1);
    width = new wxSpinCtrl(optionPanel,SPINCTRL3,"0",wxPoint(170,180),wxSize(125,34));
    width->SetRange(0,XYLimit[0]-1);
    height = new wxSpinCtrl(optionPanel,SPINCTRL4,"0",wxPoint(350,180),wxSize(125,34));
    height->SetRange(0,XYLimit[1]-1);

    alpha = new wxSpinCtrlDouble(optionPanel,SPINCTRLD,"1.0",wxPoint(170,230),wxDefaultSize,
                    wxSP_ARROW_KEYS,0.0,3.0,1.0,0.2);
    alpha->Disable();
    beta = new wxSpinCtrl(optionPanel,SPINCTRL5,"0",wxPoint(350,230),wxDefaultSize);
    beta->Disable();

    wxBoxSizer *logSizer = new wxBoxSizer(wxVERTICAL);
    logPanel->SetSizer(logSizer);
    textlog = new wxTextCtrl(logPanel,TEXTBOX,_T("Log...\n"),
                            wxPoint(0, 250), wxDefaultSize,wxTE_MULTILINE|wxTE_READONLY|wxHSCROLL);
    logSizer->Add(textlog,1,wxEXPAND | wxALL, 5);

    //static text initialization
    new wxStaticText(optionPanel,wxID_ANY,"Tipo de Filtrado:",wxPoint(10,80),wxDefaultSize);
    new wxStaticText(optionPanel,wxID_ANY,"Vertice superior izquierdo",wxPoint(170,80),wxDefaultSize);
    new wxStaticText(optionPanel,wxID_ANY,"X:",wxPoint(150,109),wxDefaultSize);
    new wxStaticText(optionPanel,wxID_ANY,"Y:",wxPoint(330,109),wxDefaultSize);
    new wxStaticText(optionPanel,wxID_ANY,"Dimension del area a operar",wxPoint(170,160),wxDefaultSize);
    new wxStaticText(optionPanel,wxID_ANY,"W:",wxPoint(150,189),wxDefaultSize);
    new wxStaticText(optionPanel,wxID_ANY,"H:",wxPoint(330,189),wxDefaultSize);
    new wxStaticText(optionPanel,wxID_ANY,wxT("α:"),wxPoint(150,239),wxDefaultSize);
    new wxStaticText(optionPanel,wxID_ANY,wxT("β:"),wxPoint(330,239),wxDefaultSize);

    //Status Message at the bottom of the window
    CreateStatusBar();
    SetStatusText("Proyecto Programacion y Algoritmos I, V1.0");

    //dynamic events binding
    Bind(wxEVT_SPINCTRL, &MyFrame::OnXULSpinChange, this,SPINCTRL1);
    Bind(wxEVT_SPINCTRL, &MyFrame::OnYULSpinChange, this,SPINCTRL2);
}

/*Write message in log panel including current event time*/
void MyFrame::setTextInLog(wxString logMessage){
    //set current time
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80];
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer,sizeof(buffer),"%H:%M:%S",timeinfo);

    //set message
    *textlog << buffer <<"-\t"<<logMessage<<".\n";
}

/*reset values on spin controls to their default states*/
void MyFrame::resetSpinCtrls(){

    alpha->SetValue(1.0);
    beta->SetValue(0);
    xUpperLeft->SetRange(0,XYLimit[0]-1);
    xUpperLeft->SetValue(0);
    yUpperLeft->SetRange(0,XYLimit[1]-1);
    yUpperLeft->SetValue(0);
    width->SetRange(0,XYLimit[0]-1);
    width->SetValue(0);
    height->SetRange(0,XYLimit[1]-1);
    height->SetValue(0);
}

/*Enable or disable Undo-Redo buttons checking their respective stack*/
void MyFrame::updateUndoRedo(){
    //check if there are operations to undo
    int stackElements = undoStack.getElements();
    if(stackElements == 0){
        undoBtn->Disable();
    }else{
        undoBtn->Enable();
    }

    //print stack state on log textbox
    wxString logMessage = wxString::Format(wxT("Operaciones en pila a descartar: %d/10"),stackElements);
    setTextInLog(logMessage);

    //check if there are operations to redo
    stackElements = undoStack.getElements();
    if(redoStack.getElements() == 0){
        redoBtn->Disable();
    }else{
        redoBtn->Enable();
    }
    
}

/*exit button on top menu*/
void MyFrame::OnExit(wxCommandEvent& event){
    Close(true);
}
 
/*about button on top menu*/
void MyFrame::OnAbout(wxCommandEvent& event){
    wxMessageBox("Proyecto final de Programación y Algoritmos I",
                 "About", wxOK | wxICON_INFORMATION);
}
 
/*Open file dialog to select files in pgm format*/
void MyFrame::OnOpen(wxCommandEvent& event){
    wxFileDialog fileDialog(this, _("Seleccione imagen PGM"), 
                            wxEmptyString, wxEmptyString, 
                            _("PGM files (*.pgm)|*.pgm|All files (*.)|*.*"),
                             wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    

    if (fileDialog.ShowModal()==wxID_OK){
        //set image
        wxString path = fileDialog.GetPath();
        drawPanel->setImage(path,wxBITMAP_TYPE_PNM);
        drawPanel->Refresh();

        //get image size
        XYLimit[0] = drawPanel->getWidth();
        XYLimit[1] = drawPanel->getHeight();
        resetSpinCtrls();
        wxString logMessage = wxString::Format(wxT("Imagen cargada (w:%d,h:%d) ruta:%s"),XYLimit[0],XYLimit[1],path);
        setTextInLog(logMessage);
        
    }else{
        wxMessageBox("Hubo un problema al cargar la imagen, revise el formato","Error", wxOK);
    }
    
}

/*Save file dialog to save new image in pgm format*/
void MyFrame::OnSave(wxCommandEvent& event){
    wxFileDialog fileDialog(this, _("Guardar imagen PGM"), 
                            wxEmptyString, wxEmptyString, 
                            _("PGM file|*.pgm|All files|*.*"), 
                            wxFD_SAVE|wxFD_OVERWRITE_PROMPT);

    if (fileDialog.ShowModal() == wxID_CANCEL)
        return;     //fileDialog canceled

    if (fileDialog.ShowModal()==wxID_OK){
        wxString path = fileDialog.GetPath();
        drawPanel->getImage().SaveFile(path,wxBITMAP_TYPE_PNM);
        wxString logMessage = wxString::Format(wxT("Imagen guardada ruta:%s"),path);
        setTextInLog(logMessage);
    }else{
        wxMessageBox("Hubo un problema al guardar la imagen, revise el formato","Error", wxOK);
    }
}

/*Undo button click*/
void MyFrame::OnButtonUndoClick(wxCommandEvent& event){

    //get last operation from the stack
    ImageProcess img_op = undoStack.pop();
    //set pre-operated patch over the whole image (parameter 0 to take original patch)
    wxImage image = drawPanel->getImage();
    drawPanel->setImage(img_op.setPatchImage(image,0),wxBITMAP_TYPE_PNM);
    drawPanel->Refresh();

    //add operation to the redostack
    redoStack.push(img_op);
    updateUndoRedo();

    //show result in log 
    wxString logMessage = wxString::Format(wxT("Operacion (%s) deshecha sobre x:%d, y:%d, base:%d, altura:%d"),
                                            filterList->GetString(img_op.getOpID()),img_op.getX(),img_op.getY(),
                                            img_op.getWidth(),img_op.getHeight());
    setTextInLog(logMessage);

    //print stack state on log textbox
    logMessage = wxString::Format(wxT("Operaciones en pila a recuperar: %d/10"),redoStack.getElements());
    setTextInLog(logMessage);
    
}

/*Redo button click*/
void MyFrame::OnButtonRedoClick(wxCommandEvent& event){
    //get last operation from the stack
    ImageProcess img_op = redoStack.pop();
    //set pre-operated patch over the whole image (parameter 1 to take operated patch)
    wxImage image = drawPanel->getImage();
    drawPanel->setImage(img_op.setPatchImage(image,1),wxBITMAP_TYPE_PNM);
    drawPanel->Refresh();

    //add operation to the undostack
    undoStack.push(img_op);
    updateUndoRedo();

    //show result in log 
    wxString logMessage = wxString::Format(wxT("Operacion (%s) recuperada sobre x:%d, y:%d, base:%d, altura:%d"),
                                            filterList->GetString(img_op.getOpID()),img_op.getX(),img_op.getY(),
                                            img_op.getWidth(),img_op.getHeight());
    setTextInLog(logMessage);
}

/*Selection made on type of processing list*/
void MyFrame::OnListBoxSelection(wxCommandEvent& event){
    //enable Apply button
    apply->Enable();

    //check if "Contraste" option was selected
    if(event.IsSelection() & filterList->IsSelected(3)){
        //enable alpha-beta spincontrols
        alpha->Enable();
        beta->Enable();
    }else if (event.IsSelection() & alpha->IsEnabled()){
        alpha->Disable();
        beta->Disable();
    }
    
}

/*Changes made on X upper left spin controls to limit square selection*/
void MyFrame::OnXULSpinChange(wxCommandEvent& event){
    //set x values for the lower Right higher or equal to this x value
    width->SetRange(0,XYLimit[0] - xUpperLeft->GetValue());
}

/*Changes made on Y upper left spin controls to limit square selection*/
void MyFrame::OnYULSpinChange(wxCommandEvent& event){
    //set x values for the lower Right higher or equal to this x value
    height->SetRange(0,XYLimit[1] - yUpperLeft->GetValue());
}


/*Click on "Aplicar" button */
void MyFrame::OnButtonApplyClick(wxCommandEvent& event){

    //get operation selected from the list
    int operation = filterList->GetSelection();

    //create operating patch with the selected square over the whole image
    int square[] = {xUpperLeft->GetValue(),yUpperLeft->GetValue(),width->GetValue(),height->GetValue()};
    wxImage image = drawPanel->getImage();
    ImageProcess img_op;

    if(square[2] == 0 & square[3] == 0){
        //operate over the whole image if both coordinates point to the same pixel
        square[2] = XYLimit[0];
        square[3] = XYLimit[1];
        img_op = ImageProcess(image,operation,0,0,XYLimit[0],XYLimit[1]);
    }else{
        img_op = ImageProcess(image,operation,square[0],square[1],square[2],square[3]);
    }
    
    //apply operation based on user's selection
    if(operation < 3)
        drawPanel->setImage((img_op.*opArr[filterList->GetSelection()])(image),wxBITMAP_TYPE_PNM);
    else
        drawPanel->setImage(img_op.constrast(image,alpha->GetValue(),beta->GetValue()),wxBITMAP_TYPE_PNM);
    drawPanel->Refresh();

    //add operation to the stack
    undoStack.push(img_op);
    updateUndoRedo();

    //show result in log 
    wxString logMessage = wxString::Format(wxT("Operacion (%s) aplicada sobre x:%d, y:%d, base:%d, altura:%d"),
                                    filterList->GetString(operation),square[0],square[1],square[2],square[3]);
    setTextInLog(logMessage);
}


////////////////////////////////////////////////////////////////////////Application Initialization (wxwidgets main)
class MyApp : public wxApp{

public:
    bool OnInit() override;//virtual function to initialize application
};
 
wxIMPLEMENT_APP(MyApp);
 
bool MyApp::OnInit(){
    wxInitAllImageHandlers();
    wxBoxSizer *sizer;
    MyFrame *frame = new MyFrame(sizer);
    frame->Show(true);
    return true;
}