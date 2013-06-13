Imports System.IO

Public Class Form1

    Private prefsFile As String = Application.StartupPath + "/shmapgui.prefs"
    Private imageName As String
    Private imageExtension As String

    Private Function LoadTGAImage(ByVal strPath As String) As Image
        Dim binReader As BinaryReader
        Dim image1 As Bitmap
        Dim type As Byte
        Dim offset As Byte
        Dim r As Byte
        Dim g As Byte
        Dim b As Byte
        Dim imgWidth As Integer, imgHeight As Integer
        Dim bpp As Byte

        If (File.Exists(strPath)) Then

            binReader = New BinaryReader(File.Open(strPath, FileMode.Open))

            ' get tga header

            offset = binReader.ReadByte() ' size of ID field that follows 18 byte header (0 usually)
            binReader.ReadByte() ' type of colour map 0=none, 1=has palette
            type = binReader.ReadByte() ' type of image 0=none, 1=indexed, 2=rgb, 3=grey, +8=rle packed
            If (type <> 0 And type <> 2) Then
                binReader.Close()
                'Return imageFallBack
                Return image1
            End If

            binReader.ReadInt16() ' first colour map entry in palette
            binReader.ReadInt16() ' number of colours in palette
            binReader.ReadByte() ' number of bits per palette entry 15,16,24,32
            binReader.ReadInt16() ' image x origin
            binReader.ReadInt16() ' image y origin

            imgWidth = binReader.ReadInt16() ' width in pixels
            imgHeight = binReader.ReadInt16() ' height in pixels

            bpp = binReader.ReadByte() ' image bits per pixel 8,16,24,32
            If (bpp < 24) Then
                Return image1
            End If
            binReader.ReadByte() ' image descriptor bits (vh flip bits)

            If (offset > 0) Then
                Dim i As Integer
                For i = 0 To offset - 1 Step 1
                    binReader.ReadByte()
                Next
            End If

            ' create empty container for the image
            image1 = New Bitmap(imgWidth, imgHeight, System.Drawing.Imaging.PixelFormat.Format24bppRgb)

            Dim x, y As Integer

            For y = imgHeight - 1 To 0 Step -1
                For x = 0 To imgWidth - 1 Step 1
                    b = binReader.ReadByte()
                    g = binReader.ReadByte()
                    r = binReader.ReadByte()
                    If (bpp > 24) Then
                        binReader.ReadByte()
                    End If

                    Dim pixelColor As Color = Color.FromArgb(r, g, b)
                    image1.SetPixel(x, y, pixelColor)
                Next
            Next

            binReader.Close()
        End If

        LoadTGAImage = image1
    End Function

    Private Function RefreshPreviewImages() As Boolean
        RefreshPreviewImages = True
        PictureBox1.Image = LoadTGAImage(imageName + "_norm.tga")
        PictureBox2.Image = LoadTGAImage(imageName + "_gloss.tga")
        PictureBox3.Image = LoadTGAImage(imageName + "_height.tga")
        PictureBox4.Image = LoadTGAImage(imageName + "_DUDV.tga")
    End Function

    Private Function OnlyNumericString(ByVal theString) As Integer
        Dim i As Integer
        Dim strNumeric As String = "0123456789"
        Dim strChar As String
        Dim CleanedString As String
        Dim negative As Boolean

        If (Mid(theString, 1, 1) = "-") Then
            negative = True
        Else
            negative = False
        End If

        CleanedString = ""
        For i = 1 To Len(theString)
            strChar = Mid(theString, i, 1)
            If InStr(strNumeric, strChar) Then
                CleanedString = CleanedString & strChar
            End If
        Next

        If (CleanedString = "") Then
            i = 0
        Else
            i = CleanedString
            If (negative) Then
                i = -i
            End If
        End If

        OnlyNumericString = i
    End Function

    Private Function CheckPath(ByVal strPath As String) As Boolean
        If (strPath <> "") Then
            If Dir$(strPath) <> "" Then
                CheckPath = True
            Else
                CheckPath = False
            End If
        Else
            CheckPath = False
        End If
    End Function

    Private Function UpdateImageNameAndExtension(ByVal strPath As String) As Boolean

        If (strPath = "") Then
            imageName = "No Image"
            imageExtension = ""
            UpdateImageNameAndExtension = False
        ElseIf (CheckPath(strPath) = False) Then
            imageName = "No Image"
            imageExtension = ""
            UpdateImageNameAndExtension = False
        ElseIf (InStr(strPath, ".") < 1) Then
            imageName = "No Image"
            imageExtension = ""
            UpdateImageNameAndExtension = False
        Else
            'remove extension from file path
            Dim nPos As Integer

            nPos = strPath.Length - 4
            imageName = strPath.Substring(0, nPos)
            imageExtension = strPath
            imageExtension = imageExtension.Remove(0, nPos)
            UpdateImageNameAndExtension = True

            RefreshPreviewImages()
        End If

    End Function

    Private Function RefreshItemDependencies() As Boolean
        GroupBox3.Enabled = CheckBox3.Checked ' Generate normal map enabled
        GroupBox5.Enabled = CheckBox4.Checked ' Generate specular map enabled
        GroupBox6.Enabled = CheckBox5.Checked ' Generate displacement map enabled
        GroupBox7.Enabled = CheckBox6.Checked ' Generate DUDV map enabled

        TextBox3.Text = HScrollBar1.Value ' normal map level
        TextBox4.Text = HScrollBar2.Value ' normal map intensity

        TextBox5.Text = HScrollBar3.Value ' specular map level
        TextBox6.Text = HScrollBar4.Value ' specular map brightness
        TextBox7.Text = HScrollBar5.Value ' specular map contrast

        TextBox8.Text = HScrollBar6.Value ' displacement map level
        TextBox9.Text = HScrollBar7.Value ' displacement map smooth
        TextBox10.Text = HScrollBar8.Value ' displacement map postblur

        TextBox11.Text = HScrollBar9.Value ' DUDV map level
    End Function

    Private Function SetNormalMapDefaults() As Boolean
        HScrollBar1.Value = 50 ' normal map level
        HScrollBar2.Value = 250 ' normal map intensity
        CheckBox1.Checked = False ' normal map invert X axis
        CheckBox2.Checked = True ' normal map invert Y axis
        SetNormalMapDefaults = True
        RefreshItemDependencies()
    End Function

    Private Function SetSpecularMapDefaults() As Boolean
        HScrollBar3.Value = 75 ' specular map level
        HScrollBar4.Value = -50 ' specular map brightness
        HScrollBar5.Value = 25 ' specular map contrast
        SetSpecularMapDefaults = True
        RefreshItemDependencies()
    End Function

    Private Function SetDisplacementMapDefaults() As Boolean
        HScrollBar6.Value = 50 ' displacement map level
        HScrollBar7.Value = 20 ' displacement map smooth
        HScrollBar8.Value = 5 ' displacement map postblur
        SetDisplacementMapDefaults = True
        RefreshItemDependencies()
    End Function

    Private Function SetDUDVMapDefaults() As Boolean
        HScrollBar9.Value = 50 ' DUDV map level
        SetDUDVMapDefaults = True
        RefreshItemDependencies()
    End Function

    Private Function ShaderMapCallGenerate() As Boolean
        Dim parms As String
        Dim fullline As String
        Dim wrapMode As String = "0"

        If (CheckBox3.Checked = False And CheckBox4.Checked = False And CheckBox5.Checked = False And CheckBox6.Checked = False) Then
            MsgBox("Nothing is selected to be generated")
            Return False
        End If

        If (CheckBox7.Checked And CheckBox8.Checked) Then
            wrapMode = "xy"
        ElseIf (CheckBox7.Checked) Then
            wrapMode = "x"
        ElseIf (CheckBox8.Checked) Then
            wrapMode = "y"
        Else
            wrapMode = "0"
        End If

        parms = "fprop -norm (TGA24,_norm) fprop -spec (TGA24,_gloss) fprop -disp (TGA24,_height) "

        fullline = Chr(34) & TextBox1.Text & Chr(34) + " " + parms

        Shell(fullline, AppWinStyle.Hide)

        parms = "cdiff " & Chr(34) & imageName + imageExtension & Chr(34) & " "

        'parms = parms + "-norm (" + normLevel.ToString + "," + normIntensity.ToString + ",*,0) -disp (*,*,*,*) -spec (*,*,*,*) "

        If (CheckBox3.Checked) Then '  normal map

            Dim invertAxis As String
            If (CheckBox1.Checked And CheckBox2.Checked) Then
                invertAxis = "xy"
            ElseIf (CheckBox1.Checked) Then
                invertAxis = "x"
            ElseIf (CheckBox2.Checked) Then
                invertAxis = "y"
            Else
                invertAxis = "0"
            End If

            parms = parms + "-norm (" + HScrollBar1.Value.ToString + "," + HScrollBar2.Value.ToString + "," + wrapMode + "," + invertAxis + ") "
        End If

        If (CheckBox5.Checked) Then '  displacement map
            parms = parms + "-disp (" + HScrollBar7.Value.ToString + "," + HScrollBar6.Value.ToString + "," + HScrollBar8.Value.ToString + "," + wrapMode + ") "
        End If

        If (CheckBox4.Checked) Then '  specular map
            parms = parms + "-spec (" + HScrollBar3.Value.ToString + "," + HScrollBar4.Value.ToString + "," + HScrollBar5.Value.ToString + "," + wrapMode + ") "
        End If

        If (CheckBox6.Checked) Then '  DUDV map
            parms = parms + "-dudv (" + HScrollBar9.Value.ToString + "," + wrapMode + ") "
        End If

        fullline = Chr(34) & TextBox1.Text & Chr(34) + " " + parms + " -v"

        Shell(fullline, AppWinStyle.NormalNoFocus)
    End Function

    Private Function ShaderMapCallPreview() As Boolean
        Dim parms As String
        Dim fullline As String
        Dim normalMapPath As String
        Dim specularMapPath As String
        Dim heightMapPath As String
        Dim DUDVMapPath As String
        Dim normal As Boolean
        Dim specular As Boolean
        Dim height As Boolean
        Dim DUDV As Boolean

        ' Launch ShaderMap preview
        normal = CheckBox3.Checked
        specular = CheckBox4.Checked
        height = CheckBox5.Checked
        DUDV = CheckBox6.Checked

        normalMapPath = imageName + "_norm.tga"
        If (normal) Then
            If (CheckPath(normalMapPath) = False) Then
                MsgBox("Can't find '" + normalMapPath + "'. Make sure to generate it before previewing")
                normal = False
            End If
        End If

        specularMapPath = imageName + "_gloss.tga"
        If (specular) Then
            If (CheckPath(specularMapPath) = False) Then
                MsgBox("Can't find '" + specularMapPath + "'. Make sure to generate it before previewing")
                specular = False
            End If
        End If

        heightMapPath = imageName + "_height.tga"
        If (height) Then
            If (CheckPath(heightMapPath) = False) Then
                MsgBox("Can't find '" + heightMapPath + "'. Make sure to generate it before previewing")
                height = False
            End If
        End If

        DUDVMapPath = imageName + "_DUDV.tga"
        If (DUDV) Then
            If (CheckPath(DUDVMapPath) = False) Then
                MsgBox("Can't find '" + DUDVMapPath + "'. Make sure to generate it before previewing")
                DUDV = False
            End If
        End If

        parms = "3dprv -diff (" + Chr(34) & imageName + imageExtension & Chr(34) + ") "
        If (normal = True) Then
            parms = parms + "-norm (" + Chr(34) & normalMapPath & Chr(34) & ") "
        End If

        If (height = True) Then
            parms = parms + "-disp (" + Chr(34) & heightMapPath & Chr(34) & ") "
        End If

        If (specular = True) Then
            parms = parms + "-spec (" + Chr(34) & specularMapPath & Chr(34) & ") "
        End If

        If (DUDV = True) Then
            parms = parms + "-dudv (" + Chr(34) & DUDVMapPath & Chr(34) & ") "
        End If

        fullline = Chr(34) & TextBox1.Text & Chr(34) + " " + parms

        Shell(fullline, vbNormalFocus)
    End Function

    Private Sub Form1_Load(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles MyBase.Load

        ' first time launched settings
        TextBox1.Text = "./shadermap.exe"
        TextBox2.Text = "no image"
        CheckBox3.Checked = True ' Generate normal map enabled
        CheckBox4.Checked = True ' Generate specular map enabled
        CheckBox5.Checked = True ' Generate displacement map enabled
        CheckBox6.Checked = False ' Generate DUDV map enabled

        CheckBox7.Checked = False ' wrap x
        CheckBox8.Checked = False ' wrap y

        SetNormalMapDefaults()
        SetSpecularMapDefaults()
        SetDisplacementMapDefaults()
        SetDUDVMapDefaults()

        If (CheckPath(prefsFile)) Then
            Dim objStreamReader As StreamReader
            Dim strLine As String
            Dim count As Integer

            'Pass the file path and the file name to the StreamReader constructor.
            objStreamReader = New StreamReader(prefsFile)

            count = 0

            'Read shadermap path.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                TextBox1.Text = strLine
            End If

            'Read last diffuse map path.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                TextBox2.Text = strLine
            End If

            'Read last generate normal map state.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                If (strLine = "yes") Then
                    CheckBox3.Checked = True
                Else
                    CheckBox3.Checked = False
                End If
            End If

            'Read last generate specular map state.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                If (strLine = "yes") Then
                    CheckBox4.Checked = True
                Else
                    CheckBox4.Checked = False
                End If
            End If

            'Read last generate displacement map state.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                If (strLine = "yes") Then
                    CheckBox5.Checked = True
                Else
                    CheckBox5.Checked = False
                End If
            End If

            'Read last generate DUDV map state.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                If (strLine = "yes") Then
                    CheckBox6.Checked = True
                Else
                    CheckBox6.Checked = False
                End If
            End If

            'Read last normal map level.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar1.Value = OnlyNumericString(strLine)
            End If

            'Read last normal map intensity.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar2.Value = OnlyNumericString(strLine)
            End If

            'Read last normal map invert X axis.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                If (strLine = "yes") Then
                    CheckBox1.Checked = True
                Else
                    CheckBox1.Checked = False
                End If
            End If

            'Read last normal map invert Y axis.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                If (strLine = "yes") Then
                    CheckBox2.Checked = True
                Else
                    CheckBox2.Checked = False
                End If
            End If

            'Read last specular map level.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar3.Value = OnlyNumericString(strLine)
            End If

            'Read last specular map brightness.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar4.Value = OnlyNumericString(strLine)
            End If

            'Read last specular map contrast.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar5.Value = OnlyNumericString(strLine)
            End If

            'Read last displacement map level.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar6.Value = OnlyNumericString(strLine)
            End If

            'Read last displacement map smooth.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar7.Value = OnlyNumericString(strLine)
            End If

            'Read last displacement map postblur.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar8.Value = OnlyNumericString(strLine)
            End If

            'Read last DUDV map level.
            strLine = objStreamReader.ReadLine
            If (Not strLine Is Nothing) Then
                HScrollBar9.Value = OnlyNumericString(strLine)
            End If

            'Close the file.
            objStreamReader.Close()
        End If

        RefreshItemDependencies()

    End Sub

    Private Sub Form1_Closing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles MyBase.FormClosing
        Dim objStreamWriter As StreamWriter

        objStreamWriter = New StreamWriter(prefsFile)

        'Write ShaderMap path.
        objStreamWriter.WriteLine(TextBox1.Text)

        'Write last diffuse map opened.
        objStreamWriter.WriteLine(TextBox2.Text)

        'Write generate normal map state.
        If (CheckBox3.Checked = True) Then
            objStreamWriter.WriteLine("yes")
        Else
            objStreamWriter.WriteLine("no")
        End If

        'Write generate specular map state.
        If (CheckBox4.Checked = True) Then
            objStreamWriter.WriteLine("yes")
        Else
            objStreamWriter.WriteLine("no")
        End If

        'Write generate displacement map state.
        If (CheckBox5.Checked = True) Then
            objStreamWriter.WriteLine("yes")
        Else
            objStreamWriter.WriteLine("no")
        End If

        'Write generate DUDV map state.
        If (CheckBox6.Checked = True) Then
            objStreamWriter.WriteLine("yes")
        Else
            objStreamWriter.WriteLine("no")
        End If

        'Write normal map level
        objStreamWriter.WriteLine(HScrollBar1.Value.ToString)

        'Write normal map intensity
        objStreamWriter.WriteLine(HScrollBar2.Value.ToString)

        'Write normal map invert X axis
        If (CheckBox1.Checked = True) Then
            objStreamWriter.WriteLine("yes")
        Else
            objStreamWriter.WriteLine("no")
        End If

        'Write normal map invert Y axis
        If (CheckBox2.Checked = True) Then
            objStreamWriter.WriteLine("yes")
        Else
            objStreamWriter.WriteLine("no")
        End If

        'specular map level
        objStreamWriter.WriteLine(HScrollBar3.Value.ToString)
        'specular map brightness
        objStreamWriter.WriteLine(HScrollBar4.Value.ToString)
        'specular map contrast
        objStreamWriter.WriteLine(HScrollBar5.Value.ToString)

        ' displacement map level
        objStreamWriter.WriteLine(HScrollBar6.Value.ToString)
        ' displacement map smooth
        objStreamWriter.WriteLine(HScrollBar7.Value.ToString)
        ' displacement map postblur
        objStreamWriter.WriteLine(HScrollBar8.Value.ToString)

        ' DUDV map level
        objStreamWriter.WriteLine(HScrollBar9.Value.ToString)

        'Close the file.
        objStreamWriter.Close()

    End Sub


    Private Sub Button1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button1.Click
        ' SHADERMAP.EXE LOAD WINDOW
        Dim fdlg As OpenFileDialog = New OpenFileDialog()
        fdlg.Title = "Locate ShaderMap Executable"
        'fdlg.InitialDirectory = "c:\"
        fdlg.Filter = "All files (*.*)|*.*|Executable files (*.exe)|*.exe"
        fdlg.FilterIndex = 2
        'fdlg.RestoreDirectory = True

        If fdlg.ShowDialog() = DialogResult.OK Then
            If (fdlg.FileName <> "") Then
                If (InStr(fdlg.FileName, "shadermap.exe") = 0) Then
                    MsgBox("Please locate shadermap.exe")
                Else
                    TextBox1.Text = fdlg.FileName
                End If
            End If
        End If
    End Sub

    Private Sub Button2_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button2.Click
        ' DIFFUSE MAP LOAD WINDOW
        Dim fdlg As OpenFileDialog = New OpenFileDialog()
        fdlg.Title = "Diffuse Image"
        'fdlg.InitialDirectory = "c:\"
        fdlg.Filter = "All files (*.*)|*.*|All files (*.*)|*.*"
        fdlg.FilterIndex = 2
        'fdlg.RestoreDirectory = True

        If fdlg.ShowDialog() = DialogResult.OK Then
            If (fdlg.FileName <> "") Then
                If (InStr(fdlg.FileName, ".jpg") <> 0) Then
                    If (UpdateImageNameAndExtension(fdlg.FileName)) Then
                        TextBox2.Text = fdlg.FileName
                    End If
                ElseIf (InStr(fdlg.FileName, ".tga") <> 0) Then
                    If (UpdateImageNameAndExtension(fdlg.FileName)) Then
                        TextBox2.Text = fdlg.FileName
                    End If
                ElseIf (InStr(fdlg.FileName, ".bmp") <> 0) Then
                    If (UpdateImageNameAndExtension(fdlg.FileName)) Then
                        TextBox2.Text = fdlg.FileName
                    End If
                Else
                    MsgBox("Image of unrecognized format")
                End If
            End If
        End If
    End Sub

    Private Sub TextBox2_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox2.TextChanged
        UpdateImageNameAndExtension(TextBox2.Text)
    End Sub

    Private Sub Button3_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button3.Click
        If (CheckPath(TextBox1.Text) = False) Then
            MsgBox("Can't find shadermap.exe, unable to execute")
        Else
            If (UpdateImageNameAndExtension(TextBox2.Text)) Then
                ' Launch ShaderMap preview
                ShaderMapCallPreview()
            Else
                MsgBox("Invalid diffuse map file assigned")
            End If
        End If
    End Sub

    Private Sub Button4_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button4.Click
        If (CheckPath(TextBox1.Text) = False) Then
            MsgBox("Can't find shadermap.exe, unable to execute")
        Else
            If (UpdateImageNameAndExtension(TextBox2.Text)) Then
                ' Launch ShaderMap conversion
                ShaderMapCallGenerate()
                Timer1.Enabled = True
            Else
                MsgBox("Invalid diffuse map file assigned")
            End If
        End If

    End Sub

    ' GENERATE MAP SWITCHES
    Private Sub CheckBox3_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles CheckBox3.CheckedChanged
        GroupBox3.Enabled = CheckBox3.Checked '  normal map
    End Sub

    Private Sub CheckBox4_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles CheckBox4.CheckedChanged
        GroupBox5.Enabled = CheckBox4.Checked '  specular map
    End Sub

    Private Sub CheckBox5_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles CheckBox5.CheckedChanged
        GroupBox6.Enabled = CheckBox5.Checked '  displacement map
    End Sub

    Private Sub CheckBox6_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles CheckBox6.CheckedChanged
        GroupBox7.Enabled = CheckBox6.Checked '  displacement map
    End Sub



    ' NORMAL MAP LEVEL
    Private Sub HScrollBar1_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar1.Scroll
        TextBox3.Text = HScrollBar1.Value.ToString
    End Sub

    Private Sub TextBox3_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox3.TextChanged
        Dim i As Integer

        If (TextBox3.Text.Length > 0) Then
            If (Mid(TextBox3.Text, 1, 1) <> "-" Or TextBox3.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox3.Text)
                If (i < HScrollBar1.Minimum) Then
                    i = HScrollBar1.Minimum
                End If
                If (i > HScrollBar1.Maximum) Then
                    i = HScrollBar1.Maximum
                End If

                HScrollBar1.Value = i
                TextBox3.Text = HScrollBar1.Value.ToString
            End If
        End If

    End Sub

    ' NORMAL MAP INTENSITY
    Private Sub HScrollBar2_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar2.Scroll
        TextBox4.Text = HScrollBar2.Value.ToString
    End Sub

    Private Sub TextBox4_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox4.TextChanged
        Dim i As Integer

        If (TextBox4.Text.Length > 0) Then
            If (Mid(TextBox4.Text, 1, 1) <> "-" Or TextBox4.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox4.Text)
                If (i < HScrollBar2.Minimum) Then
                    i = HScrollBar2.Minimum
                End If
                If (i > HScrollBar2.Maximum) Then
                    i = HScrollBar2.Maximum
                End If

                HScrollBar2.Value = i
                TextBox4.Text = HScrollBar2.Value.ToString
            End If
        End If
    End Sub

    ' SPECULAR MAP LEVEL
    Private Sub HScrollBar3_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar3.Scroll
        TextBox5.Text = HScrollBar3.Value.ToString
    End Sub

    Private Sub TextBox5_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox5.TextChanged
        Dim i As Integer

        If (TextBox5.Text.Length > 0) Then
            If (Mid(TextBox5.Text, 1, 1) <> "-" Or TextBox5.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox5.Text)
                If (i < HScrollBar3.Minimum) Then
                    i = HScrollBar3.Minimum
                End If
                If (i > HScrollBar3.Maximum) Then
                    i = HScrollBar3.Maximum
                End If

                HScrollBar3.Value = i
                TextBox5.Text = HScrollBar3.Value.ToString
            End If
        End If
    End Sub

    ' SPECULAR MAP BRIGHTNESS
    Private Sub HScrollBar4_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar4.Scroll
        TextBox6.Text = HScrollBar4.Value.ToString
    End Sub

    Private Sub TextBox6_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox6.TextChanged
        Dim i As Integer

        If (TextBox6.Text.Length > 0) Then
            If (Mid(TextBox6.Text, 1, 1) <> "-" Or TextBox6.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox6.Text)
                If (i < HScrollBar4.Minimum) Then
                    i = HScrollBar4.Minimum
                End If
                If (i > HScrollBar4.Maximum) Then
                    i = HScrollBar4.Maximum
                End If

                HScrollBar4.Value = i
                TextBox6.Text = HScrollBar4.Value.ToString
            End If
        End If
    End Sub

    ' SPECULAR MAP CONTRAST
    Private Sub HScrollBar5_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar5.Scroll
        TextBox7.Text = HScrollBar5.Value.ToString
    End Sub

    Private Sub TextBox7_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox7.TextChanged
        Dim i As Integer

        If (TextBox7.Text.Length > 0) Then
            If (Mid(TextBox7.Text, 1, 1) <> "-" Or TextBox7.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox7.Text)
                If (i < HScrollBar5.Minimum) Then
                    i = HScrollBar5.Minimum
                End If
                If (i > HScrollBar5.Maximum) Then
                    i = HScrollBar5.Maximum
                End If

                HScrollBar5.Value = i
                TextBox7.Text = HScrollBar5.Value.ToString
            End If
        End If
    End Sub

    ' DISPLACEMENT MAP LEVEL
    Private Sub HScrollBar6_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar6.Scroll
        TextBox8.Text = HScrollBar6.Value.ToString
    End Sub

    Private Sub TextBox8_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox8.TextChanged
        Dim i As Integer

        If (TextBox8.Text.Length > 0) Then
            If (Mid(TextBox8.Text, 1, 1) <> "-" Or TextBox8.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox8.Text)
                If (i < HScrollBar6.Minimum) Then
                    i = HScrollBar6.Minimum
                End If
                If (i > HScrollBar6.Maximum) Then
                    i = HScrollBar6.Maximum
                End If

                HScrollBar6.Value = i
                TextBox8.Text = HScrollBar6.Value.ToString
            End If
        End If
    End Sub

    ' DISPLACEMENT MAP SMOOTH
    Private Sub HScrollBar7_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar7.Scroll
        TextBox9.Text = HScrollBar7.Value.ToString
    End Sub

    Private Sub TextBox9_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox9.TextChanged
        Dim i As Integer

        If (TextBox9.Text.Length > 0) Then
            If (Mid(TextBox9.Text, 1, 1) <> "-" Or TextBox9.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox9.Text)
                If (i < HScrollBar7.Minimum) Then
                    i = HScrollBar7.Minimum
                End If
                If (i > HScrollBar7.Maximum) Then
                    i = HScrollBar7.Maximum
                End If

                HScrollBar7.Value = i
                TextBox9.Text = HScrollBar7.Value.ToString
            End If
        End If
    End Sub

    ' DISPLACEMENT MAP POSTBLUR
    Private Sub HScrollBar8_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar8.Scroll
        TextBox10.Text = HScrollBar8.Value.ToString
    End Sub

    Private Sub TextBox10_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox10.TextChanged
        Dim i As Integer

        If (TextBox10.Text.Length > 0) Then
            If (Mid(TextBox10.Text, 1, 1) <> "-" Or TextBox10.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox10.Text)
                If (i < HScrollBar8.Minimum) Then
                    i = HScrollBar8.Minimum
                End If
                If (i > HScrollBar8.Maximum) Then
                    i = HScrollBar8.Maximum
                End If

                HScrollBar8.Value = i
                TextBox10.Text = HScrollBar8.Value.ToString
            End If
        End If
    End Sub

    ' DUDV MAP LEVEL
    Private Sub HScrollBar9_Scroll(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ScrollEventArgs) Handles HScrollBar9.Scroll
        TextBox11.Text = HScrollBar9.Value.ToString
    End Sub

    Private Sub TextBox11_TextChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles TextBox11.TextChanged
        Dim i As Integer

        If (TextBox11.Text.Length > 0) Then
            If (Mid(TextBox11.Text, 1, 1) <> "-" Or TextBox11.Text.Length >= 2) Then
                i = OnlyNumericString(TextBox11.Text)
                If (i < HScrollBar9.Minimum) Then
                    i = HScrollBar9.Minimum
                End If
                If (i > HScrollBar9.Maximum) Then
                    i = HScrollBar9.Maximum
                End If

                HScrollBar9.Value = i
                TextBox11.Text = HScrollBar9.Value.ToString
            End If
        End If
    End Sub

    Private Sub Button5_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button5.Click
        SetNormalMapDefaults()
    End Sub

    Private Sub Button6_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button6.Click
        SetSpecularMapDefaults()
    End Sub

    Private Sub Button7_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button7.Click
        SetDisplacementMapDefaults()
    End Sub

    Private Sub Button8_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button8.Click
        SetDUDVMapDefaults()
    End Sub

    Private Sub Timer1_Tick(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Timer1.Tick
        ProgressBar1.Value = ProgressBar1.Value + 1
        If (ProgressBar1.Value >= ProgressBar1.Maximum) Then
            ProgressBar1.Value = 0
            Timer1.Enabled = False
            RefreshPreviewImages()
        End If
    End Sub

    Private Sub ProgressBar1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ProgressBar1.Click
        ProgressBar1.Value = ProgressBar1.Maximum - 1
        Timer1.Enabled = True
    End Sub

    Private Sub Button9_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button9.Click
        Process.Start("explorer.exe", "/select," + imageName + imageExtension)
    End Sub
End Class
