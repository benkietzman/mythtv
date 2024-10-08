import { Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { MenuItem, MessageService } from 'primeng/api';
import { Menu } from 'primeng/menu';
import { PartialObserver } from 'rxjs';
import { DvrService } from 'src/app/services/dvr.service';
import { UpdateRecordedMetadataRequest } from 'src/app/services/interfaces/dvr.interface';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { ProgramList } from 'src/app/services/interfaces/program.interface';
import { JobQCommands } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';
import { UtilityService } from 'src/app/services/utility.service';

@Component({
  selector: 'app-recordings',
  templateUrl: './recordings.component.html',
  styleUrls: ['./recordings.component.css'],
  providers: [MessageService]
})
export class RecordingsComponent implements OnInit {

  @ViewChild("recsform") currentForm!: NgForm;
  @ViewChild("menu") menu!: Menu;

  recordings!: ProgramList;
  JobQCmds!: JobQCommands;
  program: ScheduleOrProgram = <ScheduleOrProgram>{ Title: '' };
  editingProgram?: ScheduleOrProgram;
  displayMetadataDlg = false;
  displayRunJobs = false;
  displayUnsaved = false;
  successCount = 0;
  errorCount = 0;
  refreshing = false;

  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
    CanUndo: 'dashboard.recordings.canundel',
    AlreadyDel: 'dashboard.recordings.alreadydel'
  }

  jobsoffset = 3; // number of items before user jobs
  jobs: MenuItem[] = [
    { id: 'Transcode', label: 'dashboard.recordings.job_Transcode', command: (event) => this.runjob(event) },
    { id: 'Commflag', label: 'dashboard.recordings.job_Commflag', command: (event) => this.runjob(event) },
    { id: 'Metadata', label: 'dashboard.recordings.job_Metadata', command: (event) => this.runjob(event) },
    { id: 'UserJob1', visible: false, command: (event) => this.runjob(event) },
    { id: 'UserJob2', visible: false, command: (event) => this.runjob(event) },
    { id: 'UserJob3', visible: false, command: (event) => this.runjob(event) },
    { id: 'UserJob4', visible: false, command: (event) => this.runjob(event) }
  ];

  // Menu Items
  mnu_delete: MenuItem = { label: 'dashboard.recordings.mnu_delete', command: (event) => this.delete(event, false) };
  mnu_delete_rerec: MenuItem = { label: 'dashboard.recordings.mnu_delete_rerec', command: (event) => this.delete(event, true) };
  mnu_undelete: MenuItem = { label: 'dashboard.recordings.mnu_undelete', command: (event) => this.undelete(event) };
  mnu_rerec: MenuItem = { label: 'dashboard.recordings.mnu_rerec', command: (event) => this.rerec(event) };
  mnu_markwatched: MenuItem = { label: 'dashboard.recordings.mnu_markwatched', command: (event) => this.markwatched(event, true) };
  mnu_markunwatched: MenuItem = { label: 'dashboard.recordings.mnu_markunwatched', command: (event) => this.markwatched(event, false) };
  mnu_updatemeta: MenuItem = { label: 'dashboard.recordings.mnu_updatemeta', command: (event) => this.updatemeta(event) };
  mnu_updaterecrule: MenuItem = { label: 'dashboard.recordings.mnu_updaterecrule', command: (event) => this.updaterecrule(event) };
  mnu_stoprec: MenuItem = { label: 'dashboard.recordings.mnu_stoprec', command: (event) => this.stoprec(event) };
  mnu_runjobs: MenuItem = { label: 'dashboard.recordings.mnu_runjobs', items: this.jobs };

  menuToShow: MenuItem[] = [];

  constructor(private dvrService: DvrService, private messageService: MessageService,
    public translate: TranslateService, private setupService: SetupService,
    public utility: UtilityService) {
    this.loadRecordings();
    this.JobQCmds = this.setupService.getJobQCommands();

    // translations
    for (const [key, value] of Object.entries(this.msg)) {
      this.translate.get(value).subscribe(data => {
        Object.defineProperty(this.msg, key, { value: data });
      });
    }

    const mnu_entries = [this.mnu_delete, this.mnu_delete_rerec, this.mnu_undelete, this.mnu_rerec, this.mnu_markwatched,
    this.mnu_markunwatched, this.mnu_updatemeta, this.mnu_updaterecrule, this.mnu_stoprec, this.mnu_runjobs,
    this.jobs[0], this.jobs[1], this.jobs[2]]

    mnu_entries.forEach(entry => {
      if (entry.label)
        this.translate.get(entry.label).subscribe(data =>
          entry.label = data
        );
    });
  }

  ngOnInit(): void { }

  loadRecordings() {
    this.dvrService.GetRecordedList({}).subscribe(data => {
      this.recordings = data.ProgramList;
      this.refreshing = false;
    });
  }

  getDuration(program: ScheduleOrProgram): number {
    const starttm = new Date(program.Recording.StartTs).getTime();
    const endtm = new Date(program.Recording.EndTs).getTime();
    const duration = (endtm - starttm) / 60000;
    return duration;
  }

  // todo: add a way of showing and updating these:
  // AutoExpire, Damaged, Preserve
  showMenu(program: ScheduleOrProgram, event: any) {
    this.program = program;
    this.menuToShow.length = 0;
    if (this.program.Recording.RecGroup == 'Deleted')
      this.menuToShow.push(this.mnu_undelete);
    else {
      this.menuToShow.push(this.mnu_delete);
      this.menuToShow.push(this.mnu_delete_rerec);
    }
    this.menuToShow.push(this.mnu_rerec);
    if (program.ProgramFlagNames.indexOf('WATCHED') > -1)
      this.menuToShow.push(this.mnu_markunwatched);
    else
      this.menuToShow.push(this.mnu_markwatched);
    this.menuToShow.push(this.mnu_updatemeta);
    if (this.program.Recording.RecGroup != 'Deleted') {
      this.menuToShow.push(this.mnu_runjobs);
      for (let ix = 0; ix < 4; ix++) {
        if (this.JobQCmds.UserJob[ix]) {
          this.jobs[ix + this.jobsoffset].visible = true;
          this.jobs[ix + this.jobsoffset].label = this.JobQCmds.UserJobDesc[ix];
        }
        else
          this.jobs[ix + this.jobsoffset].visible = false;
      }
    }
    // todo: implement this
    // this.menuToShow.push(this.mnu_updaterecrule);
    this.menu.toggle(event);
  }

  delete(event: any, rerec: boolean) {
    // check with backend not already deleted by another user
    this.dvrService.GetRecorded({ RecordedId: this.program.Recording.RecordedId })
      .subscribe({
        next: (x) => {
          if (x.Program.Recording.RecGroup == 'Deleted') {
            this.sendMessage('error', event.item.label, this.msg.AlreadyDel);
            this.program.Recording.RecGroup = 'Deleted';
          }
          else {
            this.dvrService.DeleteRecording({
              RecordedId: this.program.Recording.RecordedId,
              AllowRerecord: rerec
            }).subscribe({
              next: (x) => {
                if (x.bool) {
                  this.sendMessage('success', event.item.label, this.msg.Success, this.msg.CanUndo);
                  this.program.Recording.RecGroup = 'Deleted';
                }
                else {
                  this.sendMessage('error', event.item.label, this.msg.Failed);
                }
              },
              error: (err: any) => this.networkError(err)
            });
          }
        },
        error: (err: any) => this.networkError(err)
      });
  }

  undelete(event: any) {
    this.dvrService.UnDeleteRecording({ RecordedId: this.program.Recording.RecordedId, }).subscribe({
      next: (x) => {
        if (x.bool) {
          this.program.Recording.RecGroup = 'Default';
          this.sendMessage('success', event.item.label, this.msg.Success);
        }
        else {
          this.sendMessage('error', event.item.label, this.msg.Failed);
        }
      },
      error: (err: any) => this.networkError(err)
    });
  }

  networkError(err: any) {
    console.log("network error", err);
    this.sendMessage('error', '', this.msg.NetFail);
  }

  sendMessage(severity: string, action: string, text: string, extraText?: string) {
    if (extraText)
      extraText = '\n' + extraText;
    else
      extraText = '';
    this.messageService.add({
      severity: severity, summary: text,
      detail: action + ' ' + this.program.Title + ' ' + this.program.SubTitle + extraText,
      life: 3000
      // contentStyleClass: 'recsmsg'
    });
  }

  rerec(event: any) {
    this.dvrService.AllowReRecord(this.program.Recording.RecordedId).subscribe({
      next: (x) => {
        if (x.bool)
          this.sendMessage('success', event.item.label, this.msg.Success);
        else
          this.sendMessage('error', event.item.label, this.msg.Failed);
      },
      error: (err: any) => this.networkError(err)
    });
  }

  markwatched(event: any, Watched: boolean) {
    this.dvrService.UpdateRecordedMetadata(
      { RecordedId: this.program.Recording.RecordedId, Watched: Watched }).subscribe({
        next: (x) => {
          if (x.bool) {
            this.sendMessage('success', event.item.label, this.msg.Success);
            if (Watched) {
              this.program.ProgramFlagNames = this.program.ProgramFlagNames + '|WATCHED|';
            }
            else {
              const regex = /WATCHED/g;
              this.program.ProgramFlagNames = this.program.ProgramFlagNames.replace(regex, '');
            }
          }
          else
            this.sendMessage('error', event.item.label, this.msg.Failed);
        },
        error: (err: any) => this.networkError(err)
      });
  }

  updatemeta(event: any) {
    this.editingProgram = this.program;
    this.program = Object.assign({}, this.program);
    if (this.program.Airdate)
      this.program.Airdate = new Date(this.program.Airdate + ' 00:00');
    else
      this.program.Airdate = <Date><unknown>null;
    this.displayMetadataDlg = true;
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        this.successCount++;
        this.currentForm.form.markAsPristine();
        if (this.editingProgram)
          Object.assign(this.editingProgram, this.program);
      }
      else {
        console.log("saveObserver error", x);
        this.errorCount++;
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.errorCount++;
    }
  };

  saveProgram() {
    this.successCount = 0;
    this.errorCount = 0;
    this.displayUnsaved = false;
    let request: UpdateRecordedMetadataRequest = {
      RecordedId: this.program.Recording.RecordedId,
      Description: this.program.Description,
      Episode: this.program.Episode,
      Inetref: this.program.Inetref,
      OriginalAirDate: this.program.Airdate,
      Season: this.program.Season,
      SubTitle: this.program.SubTitle,
      Title: this.program.Title
    };

    this.dvrService.UpdateRecordedMetadata(request).subscribe(this.saveObserver);

  }

  closeDialog() {
    if (this.currentForm.dirty) {
      if (this.displayUnsaved) {
        // Close on the unsaved warning
        this.displayUnsaved = false;
        this.displayMetadataDlg = false;
        this.editingProgram = undefined;
        this.currentForm.form.markAsPristine();
      }
      else
        // Close on the channel edit dialog
        // Open the unsaved warning
        this.displayUnsaved = true;
    }
    else {
      // Close on the channel edit dialog
      this.displayMetadataDlg = false;
      this.displayUnsaved = false;
      this.editingProgram = undefined;
    };

  }

  updaterecrule(event: any) { }
  stoprec(event: any) { }

  runjob(event: any) {
    this.dvrService.ManageJobQueue({
      Action: 'Add',
      JobName: event.item.id,
      RecordedId: this.program.Recording.RecordedId,
    }).subscribe({
      next: (x) => {
        if (x.int > 0) {
          this.sendMessage('success', event.item.label, this.msg.Success);
        }
        else
          this.sendMessage('error', event.item.label, this.msg.Failed);
      },
      error: (err: any) => this.networkError(err)
    });
  }

}
